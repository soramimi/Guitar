
#include "SelectAiModelDialog.h"

#include "ApplicationGlobal.h"
#include "ManageApiKeysDialog.h"
#include "MySettings.h"
#include "QueryAiModelDialog.h"
#include "SelectAiModelPresetDialog.h"
#include "common/qmisc.h"
#include "common/uuid.h"
#include "ui_SelectAiModelDialog.h"
#include <QElapsedTimer>
#include <QListWidgetItem>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <ai/AiApiBridge.h>
#include <ai/GenerativeAI.h>
#include <common/jstream.h>
#include <sys/stat.h>
#include <QClipboard>
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include <QSaveFile>
#include <QFile>

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
	bool b1 = le->blockSignals(true);
	le->setText(QString::fromStdString(text));
	deselectLineEdit(le);
	le->blockSignals(b1);
}

std::vector<ProviderID> api_choices(ProviderID provider)
{
	switch (provider) {
	case ProviderID::OpenAI: return {ProviderID::OpenAI_responses, ProviderID::OpenAI_chat_completions};
	case ProviderID::OpenAI_responses: case ProviderID::OpenAI_chat_completions:
	case ProviderID::Anthropic: case ProviderID::Google: case ProviderID::Cloudflare: return {provider};
	case ProviderID::Custom: return {ProviderID::OpenAI_responses, ProviderID::OpenAI_chat_completions, ProviderID::Anthropic, ProviderID::Google, ProviderID::Cloudflare};
	default: return {ProviderID::OpenAI_responses, ProviderID::OpenAI_chat_completions, ProviderID::Anthropic};
	}
}
void set_api_type(ModelConf &conf, ProviderID type)
{
	if (type == ProviderID::OpenAI) type = ProviderID::OpenAI_responses;
	conf.api_type = api_type_name(type);
	conf.model.api_compatibility_override = type;
}

constexpr char const *NEW_MODEL_NAME = "(New Model)";

} // namespace

class SelectAiModelDelegate : public QStyledItemDelegate {
public:
	SelectAiModelDelegate(QObject *parent = nullptr)
		: QStyledItemDelegate(parent)
	{}
	void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
	{
		QStyleOptionViewItem o = option;
		QStyledItemDelegate::initStyleOption(&o, index);
		QListWidget const *listWidget = qobject_cast<QListWidget const *>(option.widget);
		SelectAiModelDialog const *dlg = qobject_cast<SelectAiModelDialog const *>(parent());
		Q_ASSERT(dlg);
		
		QStyledItemDelegate::paint(painter, option, index);

		QColor badge_bgcolor(0, 120, 215);
		QColor badge_fgcolor = Qt::white;
		constexpr int space = 8;
		
		std::string default_model_guid = dlg->default_model_guid();
		
		GenerativeAI::ModelConf const *conf = dlg->modelconf(index.row());
		if (conf->guid == default_model_guid) {
			QString label = "def";
			int view_width = listWidget->viewport()->width();
			int label_width = painter->fontMetrics().horizontalAdvance(label) + space * 2;
			int x = view_width - label_width;
			int y = option.rect.y();
			QRect rect(x, y, label_width, option.rect.height());
			misc::drawTextBadge(painter, listWidget->palette(), rect, space, label, badge_bgcolor, badge_fgcolor, true);
		}
		
	}
};

// ダイアログの内部状態を保持する Private 構造体
struct SelectAiModelDialog::Private {
	std::vector<SelectAiModelDialog::ModelConf> items;
	std::map<QString, QString> api_key_map;
	SelectAiModelDelegate *item_delegate = nullptr;
	std::string default_model_guid;
	bool rendering = false;
	bool load_ok = false;
	bool keys_loaded = false;
	AiApiKeys keys;
	std::map<std::string, std::map<ProviderID, ModelConf>> provider_drafts;
};

SelectAiModelDialog::SelectAiModelDialog(QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::SelectAiModelDialog)
	, m(new Private)
{
	ui->setupUi(this);

	m->item_delegate = new SelectAiModelDelegate(this);
	ui->listWidget_items->setItemDelegate(m->item_delegate);
	
	// スプリッターの初期サイズを設定（左:右 = 100:300）
	ui->splitter->setSizes({100, 300});
	
	// プロバイダー一覧をコンボボックスに追加（空の tag はプレースホルダーとしてスキップ）
	for (ProviderInfo const &provider : complete_provider_table()) {
		if (provider.id == ProviderID::Invalid) continue;
		ui->comboBox_provider->addItem(QString::fromStdString(provider.description), (int)provider.id);
	}

	enableSettingsFrame(false);
}

SelectAiModelDialog::~SelectAiModelDialog()
{
	// Private データと UI オブジェクトを解放
	delete m;
	delete ui;
}

SelectAiModelDialog::ModelConf *SelectAiModelDialog::modelconf(int row)
{
	Q_ASSERT(ui->listWidget_items->count() == (int)m->items.size());
	if (row >= 0 && row < (int)m->items.size()) {
		return &m->items[row];
	}
	return nullptr;
}

SelectAiModelDialog::ModelConf *SelectAiModelDialog::current_modelconf()
{
	Q_ASSERT(ui->listWidget_items->count() == (int)m->items.size());
	int row = ui->listWidget_items->currentRow();
	return modelconf(row);
}

bool SelectAiModelDialog::set_current_modelconf(ModelConf const &newconf)
{
	Q_ASSERT(ui->listWidget_items->count() == (int)m->items.size());
	int row = ui->listWidget_items->currentRow();
	ModelConf *conf = modelconf(row);
	if (conf) {
		*conf = newconf;
		return true;
	}
	return false;
}

std::string SelectAiModelDialog::default_model_guid() const
{
	return m->default_model_guid;
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
void SelectAiModelDialog::setLineEditApiKey(std::string apikey, ModelConf const *conf)
{
	std::string account;
	
	if (conf && conf->model.provider_id() == ProviderID::Cloudflare) {
		// Cloudflare の場合、API キーは「アカウント:API キー」の形式で格納されているため、分割して表示する
		auto pos = apikey.find(':');
		if (pos != std::string::npos) {
			account = apikey.substr(0, pos);
			apikey = apikey.substr(pos + 1);
		}
	}
	
	setTextAndDeselect(ui->lineEdit_account, account);
	setTextAndDeselect(ui->lineEdit_cred_api_key, apikey);
}

GenerativeAI::Credential SelectAiModelDialog::credential() const
{
	Credential cred;

	ModelConf const *confp = current_modelconf();
	if (confp && confp->model.provider_id() == ProviderID::Cloudflare) {
		QString account = ui->lineEdit_account->text();
		QString apikey = ui->lineEdit_cred_api_key->text();		
		cred.api_key = (account + ':' + apikey).toStdString();
	} else {
		cred.api_key = ui->lineEdit_cred_api_key->text().toStdString();
	}

	return cred;
}

ProviderInfo const *SelectAiModelDialog::provider_info(GenerativeAI::ProviderID provider_id) const
{
	ProviderInfo const *provider = nullptr;
	ProviderInfo const *custom_provider = nullptr;
	
	for (GenerativeAI::ProviderInfo const &info : GenerativeAI::complete_provider_table()) {
		if (info.id == provider_id) {
			provider = &info;
			break;
		}
		if (info.id == ProviderID::Custom) {
			custom_provider = &info;
		}
	}
	
	if (!provider) {
		provider = custom_provider;
	}
	Q_ASSERT(provider);
	return provider;
}

// 「プリセット読み込み」ボタン押下時: プリセットダイアログからモデルを選択し、各入力欄に反映する
void SelectAiModelDialog::on_pushButton_load_preset_clicked()
{
	SelectAiModelPresetDialog dialog(this);
	if (dialog.exec() != QDialog::Accepted || !dialog.selectedModel()) return;
	auto *conf = current_modelconf();
	if (!conf) return;
	m->provider_drafts[conf->guid][conf->model.provider_id()] = *conf;
	conf->model = dialog.selectedModel();
	conf->name = conf->model.model_name();
	set_api_type(*conf, conf->model.api_compatibility());
	conf->api_key_symbol = provider_info(conf->model.provider_id())->env_name;
	update_api_endpoint_url();
	ui->listWidget_items->currentItem()->setText(QString::fromStdString(conf->name));
	selectItem(ui->listWidget_items->currentRow());
}

void SelectAiModelDialog::update_api_endpoint_url()
{
	ModelConf *conf = current_modelconf();
	if (!conf) return;

	std::optional<GenerativeAI::HostPort> hostport;
	if (GenerativeAI::is_endpoint_customizable(conf->model.provider_id())) {
		hostport = conf->model.hostport_; // rewrite hostport only if the provider allows customization
	}
	
	if (conf->model.provider_id() == ProviderID::Custom) {
		// nop: keep modelconf.endpoint_url
	} else {
		conf->model.set_endpoint_url({});
		Request req = make_request(conf->model.provider_id(), conf->model, {});
		if (conf->model.provider_id() == ProviderID::Google) {
			conf->model.set_endpoint_url(req.endpoint.url_);
		} else {
			Credential cred;
			conf->model.set_endpoint_url(req.endpoint.url_chat(conf->model, cred, false, hostport));
		}
	}
	setLineEditEndpointUrl(conf->model.endpoint_url());
}

// プロバイダーコンボボックス変更時: API タイプ、エンドポイント URL、API キーを切り替える
void SelectAiModelDialog::on_comboBox_provider_currentIndexChanged(int index)
{
	if (m->rendering || index < 0) return;
	auto *conf = current_modelconf(); if (!conf) return;
	auto provider = static_cast<ProviderID>(ui->comboBox_provider->itemData(index).toInt());
	if (provider == conf->model.provider_id()) return;
	auto &drafts = m->provider_drafts[conf->guid];
	drafts[conf->model.provider_id()] = *conf;
	std::string name = conf->name, guid = conf->guid;
	auto found = drafts.find(provider);
	if (found != drafts.end()) {
		*conf = found->second; conf->name = name; conf->guid = guid;
	} else {
		auto old = *conf;
		conf->model = Model(provider, old.model.model_name());
		conf->model.model_name_ = old.model.model_name();
		auto choices = api_choices(provider);
		auto type = old.model.api_compatibility();
		if (provider != ProviderID::Custom) {
			type = conf->model.api_compatibility();
			if (std::find(choices.begin(), choices.end(), type) == choices.end()) type = choices.front();
			conf->api_key_symbol = provider_info(provider)->env_name;
		}
		set_api_type(*conf, type);
		if (provider == ProviderID::Custom) conf->model.set_endpoint_url(old.model.endpoint_url());
		else update_api_endpoint_url();
	}
	selectItem(ui->listWidget_items->currentRow());
}

// API タイプコンボボックス変更時: モデルの互換性設定とエンドポイント URL を更新する
void SelectAiModelDialog::on_comboBox_api_type_currentIndexChanged(int index)
{
	if (m->rendering) return;
	if (index >= 0 && index < ui->comboBox_api_type->count()) {
		QString api_type = ui->comboBox_api_type->itemText(index);
		GenerativeAI::ProviderID api_compatibility_id = (GenerativeAI::ProviderID)ui->comboBox_api_type->itemData(index).toInt();
		
		ModelConf *conf = current_modelconf();
		if (!conf) return;
		
		conf->api_type = api_type.toStdString();
		conf->model.api_compatibility_override = api_compatibility_id;
		
		update_api_endpoint_url();
	}
}

void SelectAiModelDialog::on_lineEdit_endpoint_url_textChanged(const QString &arg1)
{
	if (m->rendering) return;
	ModelConf *conf = current_modelconf();
	if (!conf) return;
	
	conf->model.set_endpoint_url(arg1.toStdString());
}


// 認証情報の取得元（環境変数 / カスタム）が変わったときに API キー入力欄を更新する
void SelectAiModelDialog::on_cred_key_method_changed()
{
	if (m->rendering) return;
	ModelConf *conf = current_modelconf();
	if (!conf) return;
	
	std::string symbol = ui->lineEdit_cred_symbol->text().toStdString();
	conf->api_key_symbol = symbol;
	if (ui->radioButton_cred_environ->isChecked()) {
		conf->api_key_method = key_store_environment;
	} else if (ui->radioButton_cred_custom->isChecked()) {
		conf->api_key_method = key_store_encryption;
	}
	
	Credential cred;
	bool use_env = conf->api_key_method == key_store_environment;
	
	ui->lineEdit_cred_api_key->setEnabled(!use_env);
	cred.api_key = query_api_key(symbol, use_env);
	
	setLineEditApiKey(cred.api_key, conf);
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
	on_cred_key_method_changed();
}

// 「カスタム」ラジオボタン選択時: 取得元を切り替えて API キー欄を更新
void SelectAiModelDialog::on_radioButton_cred_custom_clicked()
{
	on_cred_key_method_changed();
}

// 認証シンボル（環境変数名）変更時: API キーを再取得して表示
void SelectAiModelDialog::on_lineEdit_cred_symbol_textChanged(const QString &arg1)
{
	on_cred_key_method_changed();
}

// 「Hello! テスト」ボタン押下時: 現在の設定で AI に問い合わせ、結果をメッセージボックスに表示する
void SelectAiModelDialog::on_pushButton_test_hello_clicked()
{
	AiResult result;
	
	QString prompt = "Hello!";
	
	ModelConf *conf = current_modelconf();
	if (!conf) return;
	
	QElapsedTimer timer;
	timer.start();
	
	{
		struct WaitCursor {
			WaitCursor()  { GlobalSetOverrideWaitCursor(); }
			~WaitCursor() { GlobalRestoreOverrideCursor(); }
		} defer_override_cursor;
		
		AiApiBridge api;
		GenerativeAI::Model model = conf->model;
		model.api_compatibility_override = GenerativeAI::parse_api_type(conf->api_type);
		if (GenerativeAI::is_endpoint_customizable(model.provider_id())) {
			model.set_endpoint_url(conf->model.endpoint_url());
		}
		api.set_ai_model(model, credential());
		result = api.request(prompt.toStdString(), {});
	}
	if (result.is_error()) {
		QMessageBox::warning(this, tr("Test AI Model"), tr("Error: %1").arg(QString::fromStdString(result.d.error_message)));
	} else {
		auto ms = timer.elapsed();
		QString model = QString::fromStdString(conf->model.model_name());
		std::string text = result.content();
		QMessageBox::information(this, tr("Test AI Model"), QString("--- You ---\n\n%1\n\n--- %2 ---\n\n%3\n\n--- success in %4 ms ---").arg(prompt).arg(model).arg(QString::fromStdString(text)).arg((int)ms));
	}
}


void SelectAiModelDialog::on_comboBox_model_currentTextChanged(const QString &arg1)
{
	if (m->rendering) return;
	ModelConf *conf = current_modelconf();
	if (!conf) return;
	
	conf->model.model_name_ = arg1.toStdString();
}

// 「モデル問い合わせ」ボタン押下時: プロバイダー API から利用可能なモデル一覧を取得し、選択ダイアログを表示する
void SelectAiModelDialog::on_pushButton_query_models_clicked()
{
	QString current_model_name = ui->comboBox_model->currentText();
	// 現在のプロバイダー情報とエンドポイント上書きをモデルに反映
	ModelConf *confp = current_modelconf();
	if (!confp) return;
	
	ModelConf conf = *confp;
	conf.model.provider_info_ = provider_info(conf.model.provider_id());
	conf.model.set_endpoint_url(ui->lineEdit_endpoint_url->text().toStdString());
	
	// API 経由でモデル一覧を問い合わせ（待機カーソルを表示）
	std::optional<AiResult::Models> models;
	{
		struct WaitCursor {
			WaitCursor()  { GlobalSetOverrideWaitCursor(); }
			~WaitCursor() { GlobalRestoreOverrideCursor(); }
		} defer_override_cursor;
		
		AiApiBridge api;
		api.set_ai_model(conf.model, credential());
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
		QString selected = dlg.selectedModel();
		if (selected.isEmpty()) return;
		{
			QSignalBlocker blocker(ui->comboBox_model);
			ui->comboBox_model->clear();
			for (auto const &model : models->list) ui->comboBox_model->addItem(QString::fromStdString(model.id));
			ui->comboBox_model->setCurrentText(selected);
		}
		on_comboBox_model_currentTextChanged(selected);
	}
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

void SelectAiModelDialog::selectItem(int row)
{
	QScopedValueRollback<bool> rendering(m->rendering, true);
	QSignalBlocker selection(ui->listWidget_items);
	if (ui->listWidget_items->currentRow() != row) ui->listWidget_items->setCurrentRow(row);
	auto const *conf = modelconf(row);
	enableSettingsFrame(conf != nullptr);
	if (!conf) return;
	ui->label_id->setText(QString::fromStdString(conf->guid));
	ui->lineEdit_name->setText(QString::fromStdString(conf->name));
	ui->comboBox_provider->setCurrentIndex(ui->comboBox_provider->findData(static_cast<int>(conf->model.provider_id())));
	ui->comboBox_api_type->clear();
	auto choices = api_choices(conf->model.provider_id());
	auto type = conf->model.api_compatibility();
	if (type == ProviderID::OpenAI) type = ProviderID::OpenAI_responses;
	if (std::find(choices.begin(), choices.end(), type) == choices.end()) choices.push_back(type);
	for (auto api : choices) ui->comboBox_api_type->addItem(QString::fromStdString(api_type_name(api)), static_cast<int>(api));
	ui->comboBox_api_type->setCurrentIndex(ui->comboBox_api_type->findData(static_cast<int>(type)));
	setLineEditEndpointUrl(conf->model.endpoint_url());
	ui->lineEdit_endpoint_url->setEnabled(is_endpoint_customizable(conf->model.provider_id()));
	setTextAndDeselect(ui->lineEdit_cred_symbol, conf->api_key_symbol);
	bool environment = conf->api_key_method == key_store_environment;
	ui->radioButton_cred_environ->setChecked(environment);
	ui->radioButton_cred_custom->setChecked(!environment);
	ui->lineEdit_cred_api_key->setEnabled(!environment);
	ui->lineEdit_account->setEnabled(conf->model.provider_id() == ProviderID::Cloudflare);
	setLineEditApiKey(query_api_key(conf->api_key_symbol, environment), conf);
	ui->comboBox_model->clear();
	ui->comboBox_model->setCurrentText(QString::fromStdString(conf->model.model_name()));
}

namespace {
QListWidgetItem *new_QListWidgetItem(QString const &text)
{
	auto *item = new QListWidgetItem(text);
	item->setSizeHint({24, 24});
	return item;
}
}

void SelectAiModelDialog::on_pushButton_new_clicked()
{
	int row = ui->listWidget_items->count();
	ModelConf item;
	item.model = Model(ProviderID::Custom, "");
	set_api_type(item, ProviderID::OpenAI_chat_completions);
	item.api_key_method = std::string(key_store_environment);
	item.guid = generate_uuidv7_string();
	item.name = NEW_MODEL_NAME;
	m->items.push_back(item);
	
	QListWidgetItem *list_item = new_QListWidgetItem(QString::fromStdString(item.name));
	ui->listWidget_items->addItem(list_item);
	
	selectItem(row);

	if (m->default_model_guid.empty()) {
		on_pushButton_set_as_default_clicked();
	}
	
	enableSettingsFrame(true);
}

void SelectAiModelDialog::on_pushButton_delete_clicked()
{
	int row = ui->listWidget_items->currentRow();
	if (row >= 0 && row < (int)m->items.size()) {
		auto removed = m->items[row].guid;
		m->items.erase(m->items.begin() + row);
		m->provider_drafts.erase(removed);
		if (m->default_model_guid == removed) m->default_model_guid = m->items.empty() ? "" : m->items[std::min<size_t>(row, m->items.size() - 1)].guid;

		bool b1 = ui->listWidget_items->blockSignals(true);
		delete ui->listWidget_items->takeItem(row);
		ui->listWidget_items->blockSignals(b1);
		
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
	if (m->rendering) return;
	Q_ASSERT(m->items.size() == ui->listWidget_items->count());
	
	int row = ui->listWidget_items->currentRow();
	
	ModelConf *conf = current_modelconf();
	if (conf) {
		conf->name = arg1.toStdString();
		ui->listWidget_items->item(row)->setText(arg1);
	}
}

void SelectAiModelDialog::on_listWidget_items_currentRowChanged(int currentRow)
{
	selectItem(currentRow);
}

void SelectAiModelDialog::on_lineEdit_account_textChanged(const QString &)
{
	if (!m->rendering) on_lineEdit_cred_api_key_textChanged(ui->lineEdit_cred_api_key->text());
}

void SelectAiModelDialog::on_lineEdit_cred_api_key_textChanged(const QString &arg1)
{
	if (m->rendering) return;
	ModelConf *conf = current_modelconf();
	if (conf) {
		if (conf->api_key_method != key_store_environment) {
			QString symbol = QString::fromStdString(conf->api_key_symbol);
			if (!symbol.isEmpty()) m->api_key_map[symbol] = QString::fromStdString(credential().api_key);
		}
	}
}

bool SelectAiModelDialog::load_keys()
{
	if (m->keys_loaded) return true;
	if (QFile::exists(global->app_secret_config_dir + "/apikeys.bin") && !global->load_api_keys(this, &m->keys)) return false;
	m->keys_loaded = true; return true;
}

std::string SelectAiModelDialog::query_api_key(std::string const &symbol, bool env)
{
	if (env) { auto value = std::getenv(symbol.c_str()); return value ? value : ""; }
	auto draft = m->api_key_map.find(QString::fromStdString(symbol));
	if (draft != m->api_key_map.end()) return draft->second.toStdString();
	if (!load_keys()) return {};
	auto stored = m->keys.get_api_key(symbol); return stored ? stored->api_key : "";
}

AiApiKeys SelectAiModelDialog::draft_keys() const
{
	auto keys = m->keys;
	for (auto const &[symbol, value] : m->api_key_map)
		keys.emplace(symbol.toStdString(), {AiApiKeys::KeyFrom::LocalSecret, value.toStdString()});
	return keys;
}

bool SelectAiModelDialog::save_api_keys()
{
	if (!load_keys()) return false;
	return global->save_api_keys(this, draft_keys());
}

QString SelectAiModelDialog::aimodels_json_path() const
{
	return global->aimodels_json_path();
}

bool SelectAiModelDialog::save(QWidget *)
{
	if (!m->load_ok) return false;
	auto json = ModelConf::to_json(m->items); if (!json) return false;
	QSaveFile models(aimodels_json_path());
	if (!models.open(QIODevice::WriteOnly) || models.write(json->data(), json->size()) != static_cast<qint64>(json->size())) return false;
	// Stage metadata before touching credentials, and never overwrite keys after
	// a failed read/decryption. Both files use atomic replacement.
	QString keyPath = global->app_secret_config_dir + "/apikeys.bin";
	bool hadKeys = QFile::exists(keyPath);
	QByteArray previousKeys;
	if (hadKeys) {
		QFile previous(keyPath);
		if (!previous.open(QIODevice::ReadOnly)) return false;
		previousKeys = previous.readAll();
		if (previous.error() != QFile::NoError) return false;
	}
	if (!save_api_keys()) return false;
	if (!models.commit()) {
		// Roll back credential changes if the second file cannot be committed.
		// Keep an encrypted recovery copy if the filesystem also rejects rollback.
		if (hadKeys) {
			QSaveFile rollback(keyPath);
			bool restored = rollback.open(QIODevice::WriteOnly) && rollback.setPermissions(QFile::ReadOwner | QFile::WriteOwner)
				&& rollback.write(previousKeys) == previousKeys.size() && rollback.commit();
			if (!restored) {
				QSaveFile recovery(keyPath + ".recovery");
				if (recovery.open(QIODevice::WriteOnly) && recovery.setPermissions(QFile::ReadOwner | QFile::WriteOwner)
					&& recovery.write(previousKeys) == previousKeys.size()) recovery.commit();
			}
		} else QFile::remove(keyPath);
		return false;
	}
	m->keys = draft_keys(); m->api_key_map.clear();
	return true;
}

bool SelectAiModelDialog::load(QWidget *parent)
{
	m->load_ok = false;
	auto path = aimodels_json_path();
	auto loaded = ModelConf::load(parent, path.toStdString().c_str());
	if (!loaded && QFile::exists(path)) return false;
	m->keys_loaded = false; m->keys = {};
	if (!load_keys()) return false;
	m->items = loaded ? *loaded : std::vector<ModelConf>{};
	m->provider_drafts.clear(); m->api_key_map.clear(); m->load_ok = true;
	return true;
}

void SelectAiModelDialog::updateListWidget(std::string const &def)
{
	const bool set_current = true;

	m->default_model_guid = {};
	
	int current = 0;
	
	QSignalBlocker selection(ui->listWidget_items);
	ui->listWidget_items->clear();
	for (size_t i = 0; i < m->items.size(); i++) {
		ModelConf const &item = m->items[i];
		if (item.guid == def) {
			m->default_model_guid = def;
			if (set_current) {
				current = (int)i;
			}
		}
		QString name = QString::fromStdString(item.name);
		QListWidgetItem *list_item = new_QListWidgetItem(name);
		ui->listWidget_items->addItem(list_item);
	}
	
	if (set_current) {
		selectItem(ui->listWidget_items->count() ? current : -1);
	}
}

int SelectAiModelDialog::exec(QWidget *parent)
{
	constexpr char const *kDefaultModelGUID = "DefaultModelGUID";
	
	auto *vault = global->unlockVault(parent);
	if (!vault) return QDialog::Rejected;
	
	std::string fav;
	{
		MySettings s;
		s.beginGroup("AI");
		fav = s.value(kDefaultModelGUID).toString().toStdString();
		s.endGroup();
	}
	
	if (!load(parent)) {
		QMessageBox::warning(parent, tr("AI Model Settings"), tr("Could not read model settings or API keys. Existing files have been preserved."));
		return QDialog::Rejected;
	}

	updateListWidget(fav);
	
	ui->listWidget_items->setFocus();
	
	auto ret = QDialog::exec();
	

	return ret;
}

void SelectAiModelDialog::done(int result)
{
	if (result == QDialog::Accepted) {
		if (!save(this)) {
			QMessageBox::warning(this, tr("AI Model Settings"), tr("Could not save model settings or API keys. Please retry; the dialog remains open."));
			return;
		}
		MySettings settings; settings.beginGroup("AI");
		settings.setValue("DefaultModelGUID", QString::fromStdString(m->default_model_guid));
	}
	QDialog::done(result);
}

void SelectAiModelDialog::on_pushButton_manage_api_keys_clicked()
{
	if (!load_keys()) return;
	ManageApiKeysDialog dialog(this, draft_keys());
	if (dialog.exec() == QDialog::Accepted) {
		m->keys = dialog.api_keys(); m->api_key_map.clear();
		selectItem(ui->listWidget_items->currentRow());
	}
}

void SelectAiModelDialog::on_pushButton_set_as_default_clicked()
{
	ModelConf *conf = current_modelconf();
	if (conf) {
		m->default_model_guid = conf->guid;
		ui->listWidget_items->viewport()->update();
	}
}

void SelectAiModelDialog::on_toolButton_copy_model_name_clicked()
{
	QString modelname = ui->comboBox_model->currentText();
	if (!modelname.isEmpty()) {
		QClipboard *clipboard = QGuiApplication::clipboard();
		clipboard->setText(modelname);
	}
}

