#include "MySettings.h"
#include "SelectAiModelDialog.h"

#include "ApplicationGlobal.h"
#include "QueryAiModelDialog.h"
#include "SelectAiModelPresetDialog.h"
#include "common/uuid.h"
#include "ui_SelectAiModelDialog.h"
#include <QListWidgetItem>
#include <QMenu>
#include <QMessageBox>
#include <ai/AiApiBridge.h>
#include <ai/GenerativeAI.h>
#include <common/jstream.h>

using namespace GenerativeAI;

namespace {

enum {
	ModelIndexRole = Qt::UserRole,
};

// QLineEdit の選択状態を解除し、カーソルを先頭に移動するヘルパー
void deselectLineEdit(QLineEdit *le)
{
	le->setCursorPosition(0);
	le->deselect();	
}

// QLineEdit に文字列を設定し、選択状態を解除するヘルパー
void setTextAndDeselect(QLineEdit *le, std::string const &text)
{
	le->setText(QString::fromStdString(text));
	deselectLineEdit(le);
}

constexpr char const *NEW_MODEL_NAME = "(New Model)";

} // namespace

// ダイアログの内部状態を保持する Private 構造体
struct SelectAiModelDialog::Private {
	QString generative_ai_ini_path;
	std::vector<SelectAiModelDialog::ModelConf> items;
	
	std::vector<ProviderInfo> providers;
};

SelectAiModelDialog::SelectAiModelDialog(QWidget *parent, QString generative_ai_ini_path)
	: QDialog(parent)
	, ui(new Ui::SelectAiModelDialog)
	, m(new Private)
{
	ui->setupUi(this);
	
	m->generative_ai_ini_path = generative_ai_ini_path;

	// スプリッターの初期サイズを設定（左:右 = 100:300）
	ui->splitter->setSizes({100, 300});
	
	// プロバイダー一覧をコンボボックスに追加（空の tag はプレースホルダーとしてスキップ）
	for (ProviderInfo const &provider : complete_provider_table()) {
		if (provider.tag.empty()) continue; // Skip placeholder entries
		ui->comboBox_provider->addItem(QString::fromStdString(provider.description), (int)provider.id);
	}
	
	enableSettingsFrame(false);
}

void SelectAiModelDialog::set_generative_ai_model(ModelConf const &item)
{
	Q_ASSERT(ui->listWidget_items->count() == (int)m->items.size());
	int row = ui->listWidget_items->currentRow();
	if (row >= 0 && row < (int)m->items.size()) {
		m->items[row] = std::move(item);
	}
}

std::optional<SelectAiModelDialog::ModelConf> SelectAiModelDialog::current_generative_ai_model()
{
	Q_ASSERT(ui->listWidget_items->count() == (int)m->items.size());
	int row = ui->listWidget_items->currentRow();
	if (row >= 0 && row < (int)m->items.size()) {
		return m->items[row];
	}
	return {};
}

SelectAiModelDialog::~SelectAiModelDialog()
{
	// Private データと UI オブジェクトを解放
	delete m;
	delete ui;
}

void SelectAiModelDialog::save_generative_ai_models_json()
{
	jstream::Writer w;
	w.object({}, [&](){
		w.array("items", [&](){
			for (ModelConf const &mc : m->items) {
				w.object("item", [&](){
					w.string("guid", mc.guid);
					w.string("name", mc.name);
					w.string("model", mc.model.model_name());
					w.string("api_type", mc.api_type);
					w.string("key_symbol", mc.api_key_symbol);
					w.string("key_store", mc.api_key_store);
				});
			}
		});
	});
	FILE *fp = fopen(m->generative_ai_ini_path.toStdString().c_str(), "w");
	if (fp) {
		std::string json = w;
		fwrite(json.c_str(), 1, json.size(), fp);
		fclose(fp);
	}
	
#if 1
	MySettings s;
	s.beginGroup("Options");
	s.endGroup();
#endif
}

void SelectAiModelDialog::enableSettingsFrame(bool f)
{
	ui->frame_settings->setEnabled(f);
}

// エンドポイント URL 入力欄に値を設定し、選択状態を解除する
void SelectAiModelDialog::setLineEditEndpointUrl(std::string const &url)
{
	setTextAndDeselect(ui->lineEdit_endpoint_url, url);
}

// API キー入力欄に値を設定し、選択状態を解除する
void SelectAiModelDialog::setLineEditApiKey(std::string const &apikey)
{
	setTextAndDeselect(ui->lineEdit_cred_api_key, apikey);
}

GenerativeAI::Credential SelectAiModelDialog::credential() const
{
	Credential cred;
	cred.api_key = ui->lineEdit_cred_api_key->text().toStdString();
	return cred;
}

// 「プリセット読み込み」ボタン押下時: プリセットダイアログからモデルを選択し、各入力欄に反映する
void SelectAiModelDialog::on_pushButton_load_preset_clicked()
{
	SelectAiModelPresetDialog dlg(this);
	if (dlg.exec() == QDialog::Accepted) {
		{
			ModelConf modelconf;
			modelconf.guid = generate_uuidv7();
			modelconf.name = QString::fromStdString(dlg.selectedModel().model_name()).toStdString();
			modelconf.model = dlg.selectedModel();
			set_generative_ai_model(modelconf);
		}
		
		// 選択したモデル情報を各 UI に反映
		auto opt = current_generative_ai_model();
		if (opt) {
			ModelConf modelconf = *opt;
			ui->lineEdit_name->setText(QString::fromStdString(modelconf.name));
			ui->comboBox_provider->setCurrentIndex(ui->comboBox_provider->findData((int)modelconf.model.provider_id()));
			ui->comboBox_api_type->setCurrentIndex(ui->comboBox_api_type->findData((int)modelconf.model.api_compatibility()));
			
			// エンドポイント URL を生成して表示
			modelconf.model.endpoint_url_override = {};
			Request req = make_request(modelconf.model.provider_id(), modelconf.model, {});
			Credential cred = credential();
			setLineEditEndpointUrl(req.endpoint.url_chat(modelconf.model, cred));
			
			// モデル名も comboBox_model に設定
			ui->comboBox_model->setCurrentText(QString::fromStdString(modelconf.model.model_name()));
		}
	}
}

// 「モデル問い合わせ」ボタン押下時: プロバイダー API から利用可能なモデル一覧を取得し、選択ダイアログを表示する
void SelectAiModelDialog::on_pushButton_query_models_clicked()
{
	QString current_model_name = ui->comboBox_model->currentText();
	ui->comboBox_model->clear();
	
	// 現在のプロバイダー情報とエンドポイント上書きをモデルに反映
	auto opt = current_generative_ai_model();
	if (!opt) return;
	ModelConf modelconf = *opt;
	modelconf.model.provider_info_ = provider_info(modelconf.model.provider_id());
	modelconf.model.endpoint_url_override = ui->lineEdit_endpoint_url->text().toStdString();
	
	// API 経由でモデル一覧を問い合わせ（待機カーソルを表示）
	std::optional<AiResult::Models> models;
	{
		struct WaitCursor {
			WaitCursor()  { GlobalSetOverrideWaitCursor(); }
			~WaitCursor() { GlobalRestoreOverrideCursor(); }
		} defer_override_cursor;
		
		AiApiBridge api;
		api.set_ai_model(modelconf.model, credential());
		models = api.queryModels();
	}
	if (models == std::nullopt) {
		QMessageBox::warning(this, tr("Query AI Models"), tr("Failed to query AI models. Please check your network connection and API credentials."));
		return;
	}
	
	// モデル ID でソート
	std::sort(models->list.begin(), models->list.end(), [](AiResult::Model const &a, AiResult::Model const &b) {
		return a.id < b.id;
	});

	// モデル選択ダイアログを表示
	QueryAiModelDialog dlg(this, *models, current_model_name);
	if (dlg.exec() == QDialog::Accepted) {
		// 取得したモデル一覧を comboBox_model に追加
		for (size_t i = 0; i < models->list.size(); i++) {
			AiResult::Model const &model = models->list[i];
			ui->comboBox_model->addItem(QString::fromStdString(model.id), (int)i);
		}
	
		// 選択されたモデルがあればそれを選択状態にする
		QString selected_model_name = dlg.selectedModel();
		int index = ui->comboBox_model->findText(selected_model_name);
		if (index >= 0) {
			ui->comboBox_model->setCurrentIndex(index);
		}
	}
}

// プロバイダーコンボボックス変更時: API タイプ、エンドポイント URL、API キーを切り替える
void SelectAiModelDialog::on_comboBox_provider_currentIndexChanged(int index)
{
	if (index >= 0 && index < ui->comboBox_provider->count()) {
		int i = ui->comboBox_provider->itemData(index).toInt();
		ProviderInfo const &provider = complete_provider_table()[i];
		auto opt = current_generative_ai_model();
		if (!opt) return;
		ModelConf modelconf = *opt;
		modelconf.model.provider_info_ = &provider;
		modelconf.model.api_compatibility_override = std::nullopt;
		modelconf.model.endpoint_url_override = {};
		Credential cred = global->get_ai_credential(modelconf.model);
		
		// 選択したプロバイダーに応じて利用可能な API タイプを再構築
		int index = -1;
		{
			bool b1 = ui->comboBox_api_type->blockSignals(true);
			ui->comboBox_api_type->clear();
			ui->comboBox_api_type->setCurrentIndex(-1);
			auto Add = [&](std::string_view api, ProviderID api_id) {
				ui->comboBox_api_type->addItem(QString::fromStdString(std::string(api)), QVariant((int)api_id));
			};
			switch (provider.id) {
			case ProviderID::OpenAI:
			case ProviderID::OpenAI_responses:
			case ProviderID::OpenAI_chat_completions:
				Add(api_openai_responses_v1, ProviderID::OpenAI_responses);
				Add(api_openai_chat_completions_v1, ProviderID::OpenAI_chat_completions);
				break;
			case ProviderID::Anthropic:
				Add(api_anthropic_messages_v1, ProviderID::Anthropic);
				break;
			case ProviderID::Google:
				Add(api_google_gemini_v1, ProviderID::Google);
				break;
			default:
				Add(api_openai_responses_v1, ProviderID::OpenAI_responses);
				Add(api_openai_chat_completions_v1, ProviderID::OpenAI_chat_completions);
				Add(api_anthropic_messages_v1, ProviderID::Anthropic);
				break;
			}
			index = ui->comboBox_api_type->findData((int)modelconf.model.api_compatibility());
			ui->comboBox_api_type->setCurrentIndex(index);
			ui->comboBox_api_type->blockSignals(b1);
		}
		on_comboBox_api_type_currentIndexChanged(index);
		
		// 認証情報のシンボル（環境変数名など）を表示
		ui->lineEdit_cred_symbol->setText(QString::fromStdString(provider.env_name));

		// 設定から保存済み API キーがあれば読み込み、なければ空にする
		{
			ApplicationSettings const &s = global->appsettings;
			auto it = s.ai_api_keys.map.find(provider.env_name);
			if (it != s.ai_api_keys.map.end()) {
				cred.api_key = it->second.api_key;
			} else {
				cred.api_key.clear();
			}
			
		}
		setLineEditApiKey(cred.api_key);
	}
}

// API タイプコンボボックス変更時: モデルの互換性設定とエンドポイント URL を更新する
void SelectAiModelDialog::on_comboBox_api_type_currentIndexChanged(int index)
{
	if (index >= 0 && index < ui->comboBox_api_type->count()) {
		QString api_type = ui->comboBox_api_type->itemText(index);
		ProviderID api_type_id = (ProviderID)ui->comboBox_api_type->itemData(index).toInt();
		auto opt = current_generative_ai_model();
		if (!opt) return;
		ModelConf modelconf = *opt;
		modelconf.api_type = api_type.toStdString();
		// modelconf.model.api_compatibility_override = api_type_id;
		set_generative_ai_model(modelconf);
		
		// API タイプに合わせてエンドポイント URL を再生成
		std::string url;
		Request req = make_request(modelconf.model.provider_id(), modelconf.model, {});
		if (modelconf.model.provider_id() == ProviderID::Google) {
			url = req.endpoint.url_;
		} else {
			Credential cred = global->get_ai_credential(modelconf.model);
			url = req.endpoint.url_chat(modelconf.model, cred);
		}
		setLineEditEndpointUrl(url);
	}
}

// 認証情報の取得元（環境変数 / カスタム）が変わったときに API キー入力欄を更新する
void SelectAiModelDialog::on_cred_key_store_changed()
{
	auto opt = current_generative_ai_model();
	if (!opt) return;
	ModelConf modelconf = *opt;
	Credential cred = global->get_ai_credential(modelconf.model);

	std::string symbol = ui->lineEdit_cred_symbol->text().toStdString();
	modelconf.api_key_symbol = symbol;
	if (ui->radioButton_cred_environ->isChecked()) {
		// 環境変数から API キーを取得するモード
		ui->lineEdit_cred_api_key->setEnabled(false);
		auto GetEnvironmentApiKey = [this](std::string const &symbol)-> std::string {
			char const *env = std::getenv(symbol.c_str());
			if (env) {
				return env;
			}
			return {};
		};
		cred.api_key = GetEnvironmentApiKey(symbol);
		modelconf.api_key_store = key_store_environment;
	} else if (ui->radioButton_cred_custom->isChecked()) {
		// アプリ設定から API キーを取得するモード（手動入力も可能）
		ui->lineEdit_cred_api_key->setEnabled(true);
		auto GetCustomApiKey = [this](std::string const &symbol)-> std::string {
			ApplicationSettings const &s = global->appsettings;
			int i = ui->comboBox_provider->currentIndex();
			if (i >= 0 && i < ui->comboBox_provider->count()) {
				auto it = s.ai_api_keys.map.find(symbol);
				if (it != s.ai_api_keys.map.end()) {
					return it->second.api_key;
				}
			}
			return {};
		};
		cred.api_key = GetCustomApiKey(symbol);
		modelconf.api_key_store = key_store_obfuscated;
	}
	setLineEditApiKey(cred.api_key);
	
	set_generative_ai_model(modelconf);
}

// API キー表示切替チェックボックス: 入力モードを通常表示 / パスワード表示に切り替える
void SelectAiModelDialog::on_checkBox_show_api_key_clicked()
{
	bool show = ui->checkBox_show_api_key->isChecked();
	ui->lineEdit_cred_api_key->setEchoMode(show ? QLineEdit::Normal : QLineEdit::Password);
}

// 「環境変数」ラジオボタン選択時: 取得元を切り替えて API キー欄を更新
void SelectAiModelDialog::on_radioButton_cred_environ_clicked()
{
	on_cred_key_store_changed();
}

// 「カスタム」ラジオボタン選択時: 取得元を切り替えて API キー欄を更新
void SelectAiModelDialog::on_radioButton_cred_custom_clicked()
{
	on_cred_key_store_changed();
}

// 認証シンボル（環境変数名）変更時: API キーを再取得して表示
void SelectAiModelDialog::on_lineEdit_cred_symbol_textChanged(const QString &arg1)
{
	on_cred_key_store_changed();
}

// 「Hello! テスト」ボタン押下時: 現在の設定で AI に問い合わせ、結果をメッセージボックスに表示する
void SelectAiModelDialog::on_pushButton_test_hello_clicked()
{
	AiResult result;
	
	QString prompt = "Hello!";
	auto opt = current_generative_ai_model();
	if (!opt) return;
	ModelConf modelconf = *opt;
	QString model = QString::fromStdString(modelconf.model.model_name());
	
	{
		struct WaitCursor {
			WaitCursor()  { GlobalSetOverrideWaitCursor(); }
			~WaitCursor() { GlobalRestoreOverrideCursor(); }
		} defer_override_cursor;
		
		AiApiBridge api;
		api.set_ai_model(modelconf.model, credential());
		result = api.request(prompt.toStdString());
	}
	if (result.is_error()) {
		QMessageBox::warning(this, tr("Test AI Model"), tr("Error: %1").arg(QString::fromStdString(result.d.error_message)));
	} else {
		std::string text = result.content();
		QMessageBox::information(this, tr("Test AI Model"), QString("--- You ---\n\n%1\n\n--- %2 ---\n\n%3").arg(prompt).arg(model).arg(QString::fromStdString(text)));
	}
}


void SelectAiModelDialog::on_comboBox_model_currentTextChanged(const QString &arg1)
{
	auto opt = current_generative_ai_model();
	if (!opt) return;
	ModelConf modelconf = *opt;
	modelconf.name = arg1.toStdString();
	set_generative_ai_model(modelconf);;
}

void SelectAiModelDialog::on_toolButton_clicked()
{
	QMenu menu;

	QAction *a_test = menu.addAction(tr("&Delete"), this, &SelectAiModelDialog::on_pushButton_delete_clicked);
	
	menu.show(); // 横幅を取得するために一旦表示する
	QPoint pt = ui->toolButton->mapToGlobal(QPoint(ui->toolButton->width() - menu.width(), ui->toolButton->height()));
	QAction *a = menu.exec(pt);
	
	(void)a;
}

void SelectAiModelDialog::updateListWidget()
{
	ui->listWidget_items->clear();
	for (ModelConf const &item : m->items) {
		QListWidgetItem *list_item = new QListWidgetItem(QString::fromStdString(item.name));
		ui->listWidget_items->addItem(list_item);
	}
}

void SelectAiModelDialog::selectItem(int row)
{
	Q_ASSERT(m->items.size() == ui->listWidget_items->count());
	
	if (row != ui->listWidget_items->currentRow()) {
		ui->listWidget_items->setCurrentRow(row);
	}
	
	QString name;
	QString guid;
	if (row >= 0 && row < (int)m->items.size()) {
		enableSettingsFrame(true);
		name = QString::fromStdString(m->items[row].name);
		guid = QString::fromStdString(m->items[row].guid);
	} else {
		enableSettingsFrame(false);
	}
	ui->label_id->setText(guid);
	ui->lineEdit_name->setText(name);
}

void SelectAiModelDialog::on_pushButton_new_clicked()
{
	int row = ui->listWidget_items->count();
	ModelConf item;
	{
		char tmp[37];
		auto uuid = uuidv7();
		uuid_to_string(uuid.first, uuid.second, tmp);
		item.guid = tmp;
	}
	item.name = NEW_MODEL_NAME;
	m->items.push_back(item);
	
	// updateListWidget();
	QListWidgetItem *list_item = new QListWidgetItem(QString::fromStdString(item.name));
	ui->listWidget_items->addItem(list_item);
	
	selectItem(row);
	
	enableSettingsFrame(true);
}

void SelectAiModelDialog::on_pushButton_delete_clicked()
{
	int row = ui->listWidget_items->currentRow();
	if (row >= 0 && row < (int)m->items.size()) {
		m->items.erase(m->items.begin() + row);
		
		delete ui->listWidget_items->takeItem(row);
		
		if (row < ui->listWidget_items->count()) {
			selectItem(row);
		} else if (row > 0) {
			selectItem(row - 1);
		} else {
			enableSettingsFrame(false);
		}
	}
}

void SelectAiModelDialog::on_pushButton_up_clicked()
{
	int row = ui->listWidget_items->currentRow();
	if (row > 0 && row < (int)m->items.size()) {
		std::swap(m->items[row], m->items[row - 1]);
		
		bool b1 = ui->listWidget_items->blockSignals(true);
		auto item = ui->listWidget_items->takeItem(row);
		ui->listWidget_items->insertItem(row - 1, item);
		ui->listWidget_items->blockSignals(b1);
		
		selectItem(row - 1);
	}
}

void SelectAiModelDialog::on_pushButton_down_clicked()
{
	int row = ui->listWidget_items->currentRow();
	if (row >= 0 && row + 1 < (int)m->items.size()) {
		std::swap(m->items[row], m->items[row + 1]);
		
		bool b1 = ui->listWidget_items->blockSignals(true);
		auto item = ui->listWidget_items->takeItem(row);
		ui->listWidget_items->insertItem(row + 1, item);
		ui->listWidget_items->blockSignals(b1);
		
		selectItem(row + 1);
	}
}

void SelectAiModelDialog::on_lineEdit_name_textChanged(const QString &arg1)
{
	int row = ui->listWidget_items->currentRow();
	if (row >= 0 && row < (int)m->items.size()) {
		m->items[row].name = arg1.toStdString();
		ui->listWidget_items->item(row)->setText(QString::fromStdString(m->items[row].name));
	}
}

void SelectAiModelDialog::on_listWidget_currentRowChanged(int currentRow)
{
	selectItem(currentRow);
}

