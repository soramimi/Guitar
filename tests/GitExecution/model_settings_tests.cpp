#include <gtest/gtest.h>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include "../../src/ApplicationGlobal.h"
#include "../../src/SelectAiModelDialog.h"
#include "../../src/SelectAiModelPresetDialog.h"
#include "../../src/QueryAiModelDialog.h"
#include "../../src/ManageApiKeysDialog.h"
#include "../../src/localvault/src/app/SecretVault.h"
#include "../../src/localvault/src/storage/FileBackend.h"

using namespace GenerativeAI;
namespace {
class ModelSettings : public ::testing::Test {
protected:
    QTemporaryDir directory;
    QString oldSecret, oldConfig;
    std::unique_ptr<localvault::VaultWithBackend> oldVault;
    ModelConf custom, other;
    void SetUp() override {
        ASSERT_TRUE(directory.isValid()); oldSecret = global->app_secret_config_dir; oldConfig = global->config_file_path;
        oldVault = std::move(global->vault_); global->app_secret_config_dir = directory.path(); global->config_file_path = directory.filePath("settings.ini");
        global->vault_ = std::make_unique<localvault::VaultWithBackend>();
        global->vault_->backend.backend = std::make_unique<localvault::FileBackend>(directory.filePath("vault").toStdString());
        global->vault_->vault = std::make_unique<localvault::Vault>(global->vault_->backend.backend.get());
        localvault::SecureBuffer pin("test-only", 9);
        ASSERT_EQ(global->vault_->vault->setup(pin), localvault::VaultError::None);
        ASSERT_EQ(global->vault_->vault->unlock(pin), localvault::VaultError::None);
        custom.guid = "custom-guid"; custom.name = "Custom"; custom.model = Model(ProviderID::Custom, "literal-model");
        custom.model.model_name_ = "sakura:literal-model";
        custom.model.set_endpoint_url("http://127.0.0.1:1/custom/messages");
        custom.model.api_compatibility_override = ProviderID::Anthropic;
        custom.api_type = std::string(api_anthropic_messages_v1); custom.api_key_symbol = "DUMMY_CUSTOM_SYMBOL"; custom.api_key_method = std::string(key_store_encryption);
        other = custom; other.guid = "other-guid"; other.name = "Other"; other.model.model_name_ = "other-model";
        other.api_type = std::string(api_google_gemini_v1); other.model.api_compatibility_override = ProviderID::Google;
        ASSERT_TRUE(ModelConf::save(nullptr, global->aimodels_json_path().toStdString().c_str(), {custom, other}));
        AiApiKeys keys; keys.emplace(custom.api_key_symbol, {AiApiKeys::KeyFrom::LocalSecret, "DUMMY_OLD"});
        ASSERT_TRUE(keys.save(global->vault_->vault.get()));
    }
    void TearDown() override { global->vault_ = std::move(oldVault); global->app_secret_config_dir = oldSecret; global->config_file_path = oldConfig; }
    QByteArray file(QString const &path) { QFile f(path); if (!f.open(QIODevice::ReadOnly)) return {}; return f.readAll(); }
    QByteArray models() { return file(global->aimodels_json_path()); }
    QByteArray keys() { return file(directory.filePath("apikeys.bin")); }
    template<class T> T *widget(SelectAiModelDialog &dialog, char const *name) {
        auto object = dialog.findChild<T *>(name); EXPECT_NE(object, nullptr); return object;
    }
    template<class F> int run(SelectAiModelDialog &dialog, F action) {
        QTimer::singleShot(5000, &dialog, [&dialog] { ADD_FAILURE() << "Model dialog timed out"; for (auto w : QApplication::topLevelWidgets()) if (auto d = qobject_cast<QDialog *>(w)) d->reject(); dialog.reject(); });
        QTimer::singleShot(0, &dialog, [&] { action(dialog); });
        return dialog.exec(nullptr);
    }
    std::string storedKey() { AiApiKeys loaded; EXPECT_TRUE(loaded.load(global->vault_->vault.get())); auto key = loaded.get_api_key(custom.api_key_symbol); return key ? key->api_key : ""; }
};
}
TEST_F(ModelSettings, CustomRoundTripAndRenderingPreserveEveryField)
{
    SelectAiModelDialog dialog(nullptr);
    ASSERT_EQ(run(dialog, [&](auto &d) {
        auto provider = widget<QComboBox>(d, "comboBox_provider"); auto api = widget<QComboBox>(d, "comboBox_api_type");
        auto endpoint = widget<QLineEdit>(d, "lineEdit_endpoint_url"); auto symbol = widget<QLineEdit>(d, "lineEdit_cred_symbol"); auto model = widget<QComboBox>(d, "comboBox_model");
        EXPECT_EQ(api->currentData().toInt(), static_cast<int>(ProviderID::Anthropic));
        provider->setCurrentIndex(provider->findData(static_cast<int>(ProviderID::OpenAI)));
        provider->setCurrentIndex(provider->findData(static_cast<int>(ProviderID::Custom)));
        EXPECT_EQ(endpoint->text().toStdString(), custom.model.endpoint_url()); EXPECT_EQ(symbol->text().toStdString(), custom.api_key_symbol);
        EXPECT_EQ(model->currentText().toStdString(), custom.model.model_name()); EXPECT_EQ(api->currentData().toInt(), static_cast<int>(ProviderID::Anthropic));
        auto list = widget<QListWidget>(d, "listWidget_items"); list->setCurrentRow(1);
        EXPECT_EQ(api->currentData().toInt(), static_cast<int>(ProviderID::Google)); list->setCurrentRow(0);
        QMetaObject::invokeMethod(&d, "on_pushButton_down_clicked"); QMetaObject::invokeMethod(&d, "on_pushButton_up_clicked");
        EXPECT_EQ(model->currentText().toStdString(), custom.model.model_name()); d.accept();
    }), QDialog::Accepted);
    auto loaded = ModelConf::load(nullptr, global->aimodels_json_path().toStdString().c_str()); ASSERT_TRUE(loaded); ASSERT_EQ(loaded->size(), 2u);
    auto const &actual = loaded->at(0); EXPECT_EQ(actual.model.endpoint_url(), custom.model.endpoint_url()); EXPECT_EQ(actual.api_key_symbol, custom.api_key_symbol);
    EXPECT_EQ(actual.model.model_name(), custom.model.model_name()); EXPECT_EQ(actual.model.api_compatibility(), ProviderID::Anthropic); EXPECT_EQ(actual.api_type, custom.api_type);
    EXPECT_TRUE(QJsonDocument::fromJson(models()).object().value("items").isObject());
}
TEST_F(ModelSettings, PendingKeySurvivesSelectionAndManagerCancel)
{
    auto original = keys(); SelectAiModelDialog dialog(nullptr);
    ASSERT_EQ(run(dialog, [&](auto &d) {
        auto key = widget<QLineEdit>(d, "lineEdit_cred_api_key"); key->setText("DUMMY_NEW"); auto list = widget<QListWidget>(d, "listWidget_items"); list->setCurrentRow(1); list->setCurrentRow(0);
        EXPECT_EQ(key->text(), "DUMMY_NEW");
        QTimer::singleShot(0, &d, [] { for (auto w : QApplication::topLevelWidgets()) if (auto child = qobject_cast<ManageApiKeysDialog *>(w)) child->reject(); });
        QMetaObject::invokeMethod(&d, "on_pushButton_manage_api_keys_clicked");
        EXPECT_EQ(keys(), original); EXPECT_EQ(key->text(), "DUMMY_NEW"); d.accept();
    }), QDialog::Accepted);
    EXPECT_EQ(storedKey(), "DUMMY_NEW");
}
TEST_F(ModelSettings, ManagerAcceptAndParentCancelDoNotPersistDraft)
{
    auto originalKeys = keys(), originalModels = models(); SelectAiModelDialog dialog(nullptr);
    ASSERT_EQ(run(dialog, [&](auto &d) {
        QTimer::singleShot(0, &d, [] { for (auto w : QApplication::topLevelWidgets()) if (auto child = qobject_cast<ManageApiKeysDialog *>(w)) {
            child->findChild<QTableWidget *>("tableWidget")->item(0, 1)->setText("DUMMY_CHILD"); child->accept();
        }});
        QMetaObject::invokeMethod(&d, "on_pushButton_manage_api_keys_clicked");
        EXPECT_EQ(widget<QLineEdit>(d, "lineEdit_cred_api_key")->text(), "DUMMY_CHILD"); d.reject();
    }), QDialog::Rejected);
    EXPECT_EQ(keys(), originalKeys); EXPECT_EQ(models(), originalModels);
    ASSERT_EQ(run(dialog, [&](auto &d) { EXPECT_EQ(widget<QLineEdit>(d, "lineEdit_cred_api_key")->text(), "DUMMY_OLD"); d.reject(); }), QDialog::Rejected);
}
TEST_F(ModelSettings, FailedAndCancelledQueriesDoNotClearModel)
{
    SelectAiModelDialog dialog(nullptr);
    ASSERT_EQ(run(dialog, [&](auto &d) {
        auto model = widget<QComboBox>(d, "comboBox_model");
        { QSignalBlocker blocker(model); model->addItem(QString::fromStdString(custom.model.model_name())); }
        QTimer warnings; QObject::connect(&warnings, &QTimer::timeout, &d, [] { for (auto w : QApplication::topLevelWidgets()) if (auto message = qobject_cast<QMessageBox *>(w)) message->reject(); }); warnings.start(10);
        QMetaObject::invokeMethod(&d, "on_pushButton_query_models_clicked"); warnings.stop(); EXPECT_EQ(model->currentText().toStdString(), custom.model.model_name());
        QTcpServer server; ASSERT_TRUE(server.listen(QHostAddress::LocalHost));
        QObject::connect(&server, &QTcpServer::newConnection, &server, [&] {
            auto socket = server.nextPendingConnection(); QObject::connect(socket, &QTcpSocket::readyRead, socket, [socket] {
                socket->readAll(); QByteArray body = R"({"data":[{"id":"new-model"}]})";
                socket->write("HTTP/1.1 200 OK\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body); socket->disconnectFromHost();
            });
        });
        widget<QLineEdit>(d, "lineEdit_endpoint_url")->setText("http://127.0.0.1:" + QString::number(server.serverPort()) + "/v1/chat/completions");
        QTimer cancel; QObject::connect(&cancel, &QTimer::timeout, &d, [] { for (auto w : QApplication::topLevelWidgets()) if (auto child = qobject_cast<QueryAiModelDialog *>(w)) child->reject(); }); cancel.start(10);
        QMetaObject::invokeMethod(&d, "on_pushButton_query_models_clicked"); cancel.stop(); EXPECT_EQ(model->currentText().toStdString(), custom.model.model_name()); d.reject();
    }), QDialog::Rejected);
}
TEST_F(ModelSettings, PresetSynchronizesProtocolBeforeSave)
{
    SelectAiModelDialog dialog(nullptr); Model selected;
    ASSERT_EQ(run(dialog, [&](auto &d) {
        QTimer::singleShot(0, &d, [&] { for (auto w : QApplication::topLevelWidgets()) if (auto child = qobject_cast<SelectAiModelPresetDialog *>(w)) {
            child->findChild<QTableWidget *>("tableWidget")->setCurrentCell(0, 0); selected = child->selectedModel(); child->accept();
        }});
        QMetaObject::invokeMethod(&d, "on_pushButton_load_preset_clicked"); ASSERT_TRUE(static_cast<bool>(selected));
        EXPECT_EQ(widget<QComboBox>(d, "comboBox_api_type")->currentText().toStdString(), api_type_name(selected.api_compatibility())); d.accept();
    }), QDialog::Accepted);
    auto loaded = ModelConf::load(nullptr, global->aimodels_json_path().toStdString().c_str()); ASSERT_TRUE(loaded);
    EXPECT_EQ(loaded->at(0).api_type, api_type_name(selected.api_compatibility()));
}
TEST_F(ModelSettings, EncryptionFailurePreservesBothExistingFiles)
{
    auto originalKeys = keys(), originalModels = models(); AiApiKeys draft; draft.emplace(custom.api_key_symbol, {AiApiKeys::KeyFrom::LocalSecret, "DUMMY_NEW"});
    global->vault_->vault->lock(); EXPECT_FALSE(draft.save(global->vault_->vault.get())); EXPECT_EQ(keys(), originalKeys); EXPECT_EQ(models(), originalModels);
    localvault::SecureBuffer pin("test-only", 9); ASSERT_EQ(global->vault_->vault->unlock(pin), localvault::VaultError::None);
    SelectAiModelDialog dialog(nullptr);
    EXPECT_EQ(run(dialog, [&](auto &d) {
        widget<QLineEdit>(d, "lineEdit_cred_api_key")->setText("DUMMY_NEW"); global->vault_->vault->lock();
        QTimer::singleShot(0, &d, [] { for (auto w : QApplication::topLevelWidgets()) if (auto message = qobject_cast<QMessageBox *>(w)) message->reject(); });
        d.accept(); EXPECT_TRUE(d.isVisible()); EXPECT_EQ(keys(), originalKeys); EXPECT_EQ(models(), originalModels); d.reject();
    }), QDialog::Rejected);
}
TEST_F(ModelSettings, CorruptInputBlocksSaveAndLegacyRecordsDoNotInheritFields)
{
    QFile file(global->aimodels_json_path()); ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(R"({"items":{"item":{"guid":"one","provider":"custom","model":"first","endpoint_url":"http://first.invalid"},"item":{"guid":"two","provider":"custom","model":"second"}}})"); file.close();
    auto loaded = ModelConf::load(nullptr, file.fileName().toStdString().c_str()); ASSERT_TRUE(loaded); ASSERT_EQ(loaded->size(), 2u); EXPECT_TRUE(loaded->at(1).model.endpoint_url().empty());
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate)); file.write("not json"); file.close(); auto corrupt = models(); auto originalKeys = keys();
    EXPECT_FALSE(ModelConf::load(nullptr, file.fileName().toStdString().c_str()));
    SelectAiModelDialog dialog(nullptr); EXPECT_FALSE(dialog.load(nullptr)); EXPECT_FALSE(dialog.save(nullptr)); EXPECT_EQ(models(), corrupt); EXPECT_EQ(keys(), originalKeys);
}
TEST_F(ModelSettings, FailedMetadataStageDoesNotSaveCredentials)
{
    auto originalKeys = keys(); SelectAiModelDialog dialog(nullptr); ASSERT_TRUE(dialog.load(nullptr));
    ASSERT_TRUE(QFile::remove(global->aimodels_json_path())); ASSERT_TRUE(QDir().mkdir(global->aimodels_json_path()));
    EXPECT_FALSE(dialog.save(nullptr)); EXPECT_EQ(keys(), originalKeys);
}
