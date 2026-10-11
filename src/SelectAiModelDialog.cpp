
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

// 「プリセット読み込み」ボタン押下時: プリセットダイアログからモデルを選択し、各入力欄に反映する
void SelectAiModelDialog::on_pushButton_load_preset_clicked()
{
	SelectAiModelPresetDialog dlg(this);
	if (dlg.exec() == QDialog::Accepted) {
		ModelConf *confp = current_modelconf();
		{
			ModelConf conf;
			conf.guid = confp ? confp->guid : generate_uuidv7_string();
			conf.name = QString::fromStdString(dlg.selectedModel().model_name()).toStdString();
			conf.model = dlg.selectedModel();
			set_current_modelconf(conf);
		}
		
		// 選択したモデル情報を各 UI に反映
		if (confp) {
			ui->lineEdit_name->setText(QString::fromStdString(confp->name));
			ui->comboBox_provider->setCurrentIndex(ui->comboBox_provider->findData((int)confp->model.provider_id()));
			ui->comboBox_api_type->setCurrentIndex(ui->comboBox_api_type->findData((int)confp->model.api_compatibility()));
			
			// エンドポイント URL を生成して表示
			confp->model.set_endpoint_url({});
			Request req = make_request(confp->model.provider_id(), confp->model, {});
			Credential cred = credential();
			setLineEditEndpointUrl(req.endpoint.url_chat(confp->model, cred, false));
			
			// モデル名も comboBox_model に設定
			ui->comboBox_model->setCurrentText(QString::fromStdString(confp->model.model_name()));
		}
	}
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
			Credential cred = global->get_ai_credential(conf->model);
			conf->model.set_endpoint_url(req.endpoint.url_chat(conf->model, cred, false, hostport));
		}
	}
	setLineEditEndpointUrl(conf->model.endpoint_url());
}

// プロバイダーコンボボックス変更時: API タイプ、エンドポイント URL、API キーを切り替える
void SelectAiModelDialog::on_comboBox_provider_currentIndexChanged(int index)
{
	if (index >= 0 && index < ui->comboBox_provider->count()) {
		GenerativeAI::ProviderID provider_id = (GenerativeAI::ProviderID)ui->comboBox_provider->itemData(index).toInt();
		ProviderInfo const *provider = nullptr;
		for (GenerativeAI::ProviderInfo const &info : GenerativeAI::complete_provider_table()) {
			if (info.id == provider_id) {
				provider = &info;
				break;
			}
		}
		if (!provider) return;
		
		ModelConf *conf = current_modelconf();
		if (!conf) return;
		
		ProviderID apitype = GenerativeAI::parse_api_type(conf->api_type);
		
		std::string save_ep_url = conf->model.endpoint_url();
		
		conf->model.provider_info_ = provider;
		conf->model.api_compatibility_override = std::nullopt;
		conf->model.set_endpoint_url({});
		
		{
			// 選択したプロバイダーに応じて利用可能な API タイプを再構築
			int index = -1;
			{
				bool b1 = ui->comboBox_api_type->blockSignals(true);
				ui->comboBox_api_type->clear();
				ui->comboBox_api_type->setCurrentIndex(-1);
				auto Add = [&](std::string_view api, ProviderID api_id) {
					ui->comboBox_api_type->addItem(QString::fromStdString(std::string(api)), QVariant((int)api_id));
				};
				switch (provider->id) {
				case ProviderID::OpenAI:
					Add(api_openai_responses_v1, ProviderID::OpenAI_responses);
					Add(api_openai_chat_completions_v1, ProviderID::OpenAI_chat_completions);
					break;
				case ProviderID::OpenAI_responses:
					Add(api_openai_responses_v1, ProviderID::OpenAI_responses);
					break;
				case ProviderID::OpenAI_chat_completions:
					Add(api_openai_chat_completions_v1, ProviderID::OpenAI_chat_completions);
					break;
				case ProviderID::Anthropic:
					Add(api_anthropic_messages_v1, ProviderID::Anthropic);
					break;
				case ProviderID::Google:
					Add(api_google_gemini_v1, ProviderID::Google);
					break;
				case ProviderID::Cloudflare:
					Add(api_cloudflare_gateway_v4, ProviderID::Cloudflare);
					break;
				default:
					Add(api_openai_responses_v1, ProviderID::OpenAI_responses);
					Add(api_openai_chat_completions_v1, ProviderID::OpenAI_chat_completions);
					Add(api_anthropic_messages_v1, ProviderID::Anthropic);
					break;
				}
				index = ui->comboBox_api_type->findData((int)apitype);
				ui->comboBox_api_type->setCurrentIndex(index);
				ui->comboBox_api_type->blockSignals(b1);
			}
		}
		
		if (GenerativeAI::is_endpoint_customizable(provider_id)) {
			conf->model.set_endpoint_url(save_ep_url);
			ui->lineEdit_endpoint_url->setEnabled(true);
		} else {
			update_api_endpoint_url();
			ui->lineEdit_endpoint_url->setEnabled(false);
		}
		
		// 認証情報のシンボル（環境変数名など）を表示
		ui->lineEdit_cred_symbol->setText(QString::fromStdString(provider->env_name));

		Credential cred = global->get_ai_credential(conf->model);
		bool use_env = conf->api_key_method == key_store_environment;
		cred.api_key = query_api_key(provider->env_name, use_env);
		setLineEditApiKey(cred.api_key, conf);
	}
}

// API タイプコンボボックス変更時: モデルの互換性設定とエンドポイント URL を更新する
void SelectAiModelDialog::on_comboBox_api_type_currentIndexChanged(int index)
{
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
	ModelConf *conf = current_modelconf();
	if (!conf) return;
	
	conf->model.set_endpoint_url(arg1.toStdString());
}


// 認証情報の取得元（環境変数 / カスタム）が変わったときに API キー入力欄を更新する
void SelectAiModelDialog::on_cred_key_method_changed()
{
	ModelConf *conf = current_modelconf();
	if (!conf) return;
	
	std::string symbol = ui->lineEdit_cred_symbol->text().toStdString();
	conf->api_key_symbol = symbol;
	if (ui->radioButton_cred_environ->isChecked()) {
		conf->api_key_method = key_store_environment;
	} else if (ui->radioButton_cred_custom->isChecked()) {
		conf->api_key_method = key_store_encryption;
	}
	
	Credential cred = global->get_ai_credential(conf->model);
	
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
		result = api.request(prompt.toStdString());
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
	ModelConf *conf = current_modelconf();
	if (!conf) return;
	
	conf->model.model_name_ = arg1.toStdString();
}

// 「モデル問い合わせ」ボタン押下時: プロバイダー API から利用可能なモデル一覧を取得し、選択ダイアログを表示する
void SelectAiModelDialog::on_pushButton_query_models_clicked()
{
	QString current_model_name = ui->comboBox_model->currentText();
	ui->comboBox_model->clear();
	
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
		// 取得したモデル一覧を comboBox_model に追加
		bool b1 = ui->comboBox_model->blockSignals(true);
		for (size_t i = 0; i < models->list.size(); i++) {
			AiResult::Model const &model = models->list[i];
			ui->comboBox_model->addItem(QString::fromStdString(model.id), (int)i);
		}
		ui->comboBox_model->blockSignals(b1);
	
		// 選択されたモデルがあればそれを選択状態にする
		QString selected_model_name = dlg.selectedModel();
		int index = ui->comboBox_model->findText(selected_model_name);
		if (index >= 0) {
			ui->comboBox_model->setCurrentIndex(index);
			// *p = conf;
		}
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
	Q_ASSERT(m->items.size() == ui->listWidget_items->count());
	
	if (row != ui->listWidget_items->currentRow()) {
		ui->listWidget_items->setCurrentRow(row);
	}
	
	ModelConf conf;
	{
		QString name;
		QString guid;
		
		ModelConf const *confp = modelconf(row);
		if (confp) {
			conf = *confp; // この時点での設定を取得（下のsetTextなどのイベントで、ポインタの先が書き換えられるので）
			
			enableSettingsFrame(true);
			name = QString::fromStdString(conf.name);
			guid = QString::fromStdString(conf.guid);
		} else {
			enableSettingsFrame(false);
		}
		
		ui->label_id->setText(guid);
		ui->lineEdit_name->setText(name);
	}
	
	ui->comboBox_provider->setCurrentText(QString::fromStdString(conf.model.provider_description()));
	
	ui->comboBox_api_type->setCurrentText(QString::fromStdString(conf.api_type));
	
	ui->lineEdit_endpoint_url->setText(QString::fromStdString(conf.model.endpoint_url()));
	
	setTextAndDeselect(ui->lineEdit_cred_symbol, conf.api_key_symbol);
	ui->lineEdit_cred_symbol->setText(QString::fromStdString(conf.api_key_symbol));
	
	if (conf.api_key_method == key_store_environment) {
		ui->radioButton_cred_environ->setChecked(true);
	} else {
		ui->radioButton_cred_custom->setChecked(true);
	}
	
	ui->lineEdit_account->setEnabled(conf.model.provider_id() == ProviderID::Cloudflare);
	
	ui->comboBox_model->setCurrentText(QString::fromStdString(conf.model.model_name()));
	
	on_cred_key_method_changed();
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
		m->items.erase(m->items.begin() + row);

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

void SelectAiModelDialog::on_lineEdit_cred_symbol_textEdited(const QString &arg1)
{
	ModelConf *conf = current_modelconf();
	if (conf) {
		conf->api_key_symbol = arg1.toStdString();
		on_cred_key_method_changed();
	}
}

void SelectAiModelDialog::on_lineEdit_cred_api_key_textChanged(const QString &arg1)
{
	ModelConf *conf = current_modelconf();
	if (conf) {
		if (conf->api_key_method != key_store_environment) {
			QString symbol = QString::fromStdString(conf->api_key_symbol);
			m->api_key_map[symbol] = arg1;
		}
	}
}

std::string SelectAiModelDialog::query_api_key(std::string const &symbol, bool env)
{
	if (env) {
		char const *e = std::getenv(symbol.c_str());
		if (e) return e;
	} else {
		AiApiKeys ai_api_keys;
		global->load_api_keys(this, &ai_api_keys);
		auto opt = ai_api_keys.get_api_key(symbol);
		if (opt) return opt->api_key;
	}
	return {};
}

void SelectAiModelDialog::save_api_keys()
{
	std::vector<ModelConf> const &items = m->items;
	std::map<QString, QString> const &api_key_map = m->api_key_map;
	
	AiApiKeys ai_api_keys;
	global->load_api_keys(this, &ai_api_keys);
	
	for (SelectAiModelDialog::ModelConf const &conf : items) {
		if (conf.api_key_method == key_store_environment) {
			// nop: 環境変数から取得する場合は保存しない
		} else {
			QString symbol = QString::fromStdString(conf.api_key_symbol);
			auto it = api_key_map.find(symbol);
			if (it != api_key_map.end()) {
				AiApiKeys::Item item;
				item.from = AiApiKeys::KeyFrom::LocalSecret;
				item.api_key = it->second.toStdString();
				ai_api_keys.emplace(symbol.toStdString(), item);
			}
		}
	}
	
	global->save_api_keys(this, ai_api_keys);
}

QString SelectAiModelDialog::aimodels_json_path() const
{
	return global->aimodels_json_path();
}

void SelectAiModelDialog::save(QWidget *parent)
{
	GenerativeAI::ModelConf::save(parent, aimodels_json_path().toStdString().c_str(), m->items);
	save_api_keys();
}

void SelectAiModelDialog::load(QWidget *parent)
{
	auto opt = GenerativeAI::ModelConf::load(parent, aimodels_json_path().toStdString().c_str());
	if (opt) {
		m->items = *opt;
	}
}

void SelectAiModelDialog::updateListWidget(std::string const &def)
{
	const bool set_current = true;

	m->default_model_guid = {};
	
	int current = 0;
	
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
		ui->listWidget_items->setCurrentRow(current);
	}
}

int SelectAiModelDialog::exec(QWidget *parent)
{
	constexpr char const *kDefaultModelGUID = "DefaultModelGUID";
	
	localvault::Vault *vault = global->unlockVault(parent);
	if (!vault) return QDialog::Rejected;
	
	std::string fav;
	{
		MySettings s;
		s.beginGroup("AI");
		fav = s.value(kDefaultModelGUID).toString().toStdString();
		s.endGroup();
	}
	
	load(parent);

	updateListWidget(fav);
	
	ui->listWidget_items->setFocus();
	
	auto ret = QDialog::exec();
	
	if (ret == QDialog::Accepted) {
		save(parent);

		MySettings s;
		s.beginGroup("AI");
		s.setValue(kDefaultModelGUID, QString::fromStdString(m->default_model_guid));
		s.endGroup();
	}
	
	return ret;
}

void SelectAiModelDialog::on_pushButton_manage_api_keys_clicked()
{
	save_api_keys(); // 現在までの変更を確定
	
	AiApiKeys ai_api_keys;
	global->load_api_keys(this, &ai_api_keys);
	
	ManageApiKeysDialog dlg(this, ai_api_keys);
	if (dlg.exec() == QDialog::Accepted) {
		ai_api_keys = dlg.api_keys();
		global->save_api_keys(this, ai_api_keys);
		on_cred_key_method_changed();
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

