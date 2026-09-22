#include "ApplicationGlobal.h"
#include "QueryAiModelDialog.h"
#include "SelectAiModelDialog.h"
#include "ui_SelectAiModelDialog.h"

#include "SelectAiModelPresetDialog.h"
#include <ai/AiApiBridge.h>
#include <ai/GenerativeAI.h>
#include <QListWidgetItem>
#include <QMessageBox>

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

} // namespace

// ダイアログの内部状態を保持する Private 構造体
struct SelectAiModelDialog::Private {
	std::vector<ProviderInfo> providers;
	GenerativeAI::Model model;
};

SelectAiModelDialog::SelectAiModelDialog(QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::SelectAiModelDialog)
	, m(new Private)
{
	ui->setupUi(this);

	// スプリッターの初期サイズを設定（左:右 = 100:300）
	ui->splitter->setSizes({100, 300});
	
	// プロバイダー一覧をコンボボックスに追加（空の tag はプレースホルダーとしてスキップ）
	for (ProviderInfo const &provider : complete_provider_table()) {
		if (provider.tag.empty()) continue; // Skip placeholder entries
		ui->comboBox_provider->addItem(QString::fromStdString(provider.description), (int)provider.id);
	}
	
	// 利用可能な API タイプ名を定義
	static constexpr std::string_view api_openai_chat_completions_v1 = "openai_chat_completions_v1";
	static constexpr std::string_view api_openai_responses_v1 = "openai_responses_v1";
	static constexpr std::string_view api_anthropic_messages_v1 = "anthropic_messages_v1";
	static constexpr std::string_view api_google_gemini_v1 = "google_gemini_v1";
	
	// API タイプをコンボボックスに追加するラムダ
	auto AddApiType = [this](std::string_view const &api_type_name, ProviderID api_type_id) {
		QString api_type_text = QString::fromStdString((std::string)api_type_name);
		ui->comboBox_api_type->addItem(api_type_text, (int)api_type_id);
		qDebug() << api_type_text << (int)api_type_id;
	};
	AddApiType(api_openai_chat_completions_v1, ProviderID::OpenAI_chat_completions);
	AddApiType(api_openai_responses_v1, ProviderID::OpenAI_responses);
	AddApiType(api_anthropic_messages_v1, ProviderID::Anthropic);
	AddApiType(api_google_gemini_v1, ProviderID::Google);
}

SelectAiModelDialog::~SelectAiModelDialog()
{
	// Private データと UI オブジェクトを解放
	delete m;
	delete ui;
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

// 「プリセット読み込み」ボタン押下時: プリセットダイアログからモデルを選択し、各入力欄に反映する
void SelectAiModelDialog::on_pushButton_load_preset_clicked()
{
	SelectAiModelPresetDialog dlg(this);
	if (dlg.exec() == QDialog::Accepted) {
		m->model = dlg.selectedModel();
		Model const &model = m->model;
		
		// 選択したモデル情報を各 UI に反映
		ui->lineEdit_name->setText(QString::fromStdString(m->model.model_name()));
		ui->comboBox_provider->setCurrentIndex(ui->comboBox_provider->findData((int)model.provider_id()));
		ui->comboBox_api_type->setCurrentIndex(ui->comboBox_api_type->findData((int)model.api_compatibility()));
		
		// エンドポイント URL を生成して表示
		Request req = make_request(model.provider_id(), model, {});
		setLineEditEndpointUrl(req.endpoint.url_chat());
		
		// モデル名も comboBox_model に設定
		ui->comboBox_model->setCurrentText(QString::fromStdString(model.model_name()));
	}
}

// 「モデル問い合わせ」ボタン押下時: プロバイダー API から利用可能なモデル一覧を取得し、選択ダイアログを表示する
void SelectAiModelDialog::on_pushButton_query_model_clicked()
{
	QString current_model_name = ui->comboBox_model->currentText();
	ui->comboBox_model->clear();
	
	// 現在のプロバイダー情報とエンドポイント上書きをモデルに反映
	m->model.provider_info_ = provider_info(m->model.provider_id());
	m->model.endpoint_url_override = ui->lineEdit_endpoint_url->text().toStdString();

	// API 経由でモデル一覧を問い合わせ（待機カーソルを表示）
	std::optional<AiResult::Models> models;
	{
		struct WaitCursor {
			WaitCursor()  { GlobalSetOverrideWaitCursor(); }
			~WaitCursor() { GlobalRestoreOverrideCursor(); }
		} defer_override_cursor;
		
		AiApiBridge api;
		api.set_ai_model(m->model);
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
		m->model.provider_info_ = &provider;
		m->model.api_compatibility_override = std::nullopt;
		Credential cred = global->get_ai_credential(m->model);
		
		// 選択したプロバイダーに応じて利用可能な API タイプを再構築
		{
			static constexpr std::string_view api_openai_chat_completions_v1 = "openai_chat_completions_v1";
			static constexpr std::string_view api_openai_responses_v1 = "openai_responses_v1";
			static constexpr std::string_view api_anthropic_messages_v1 = "anthropic_messages_v1";
			static constexpr std::string_view api_google_gemini_v1 = "google_gemini_v1";

			ui->comboBox_api_type->clear();
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
		}
		ui->comboBox_api_type->setCurrentIndex(ui->comboBox_api_type->findData((int)m->model.api_compatibility()));
		
		// エンドポイント URL を更新
		Request req = GenerativeAI::make_request(provider.id, m->model, cred);
		setLineEditEndpointUrl(req.endpoint.url_chat());
		
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
		ProviderID api_type_id = (ProviderID)ui->comboBox_api_type->itemData(index).toInt();
		m->model.api_compatibility_override = api_type_id;
		
		// API タイプに合わせてエンドポイント URL を再生成
		Request req = make_request(m->model.provider_id(), m->model, {});
		setLineEditEndpointUrl(req.endpoint.url_chat());
	}
}

// 認証情報の取得元（環境変数 / カスタム）が変わったときに API キー入力欄を更新する
void SelectAiModelDialog::on_cred_key_source_changed()
{
	Credential cred = global->get_ai_credential(m->model);

	std::string symbol = ui->lineEdit_cred_symbol->text().toStdString();
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
	}
	setLineEditApiKey(cred.api_key);
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
	on_cred_key_source_changed();
}

// 「カスタム」ラジオボタン選択時: 取得元を切り替えて API キー欄を更新
void SelectAiModelDialog::on_radioButton_cred_custom_clicked()
{
	on_cred_key_source_changed();
}

// 認証シンボル（環境変数名）変更時: API キーを再取得して表示
void SelectAiModelDialog::on_lineEdit_cred_symbol_textChanged(const QString &arg1)
{
	on_cred_key_source_changed();
}

// 「Hello! テスト」ボタン押下時: 現在の設定で AI に問い合わせ、結果をメッセージボックスに表示する
void SelectAiModelDialog::on_pushButton_test_hello_clicked()
{
	AiResult result;
	{
		struct WaitCursor {
			WaitCursor()  { GlobalSetOverrideWaitCursor(); }
			~WaitCursor() { GlobalRestoreOverrideCursor(); }
		} defer_override_cursor;
		
		AiApiBridge api;
		api.set_ai_model(m->model);
		result = api.request("Hello!");
	}
	if (result.is_error()) {
		QMessageBox::warning(this, tr("Test AI Model"), tr("Error: %1").arg(QString::fromStdString(result.d.error_message)));
	} else {
		std::string text = result.content();
		QMessageBox::information(this, tr("Test AI Model"), tr("AI Response:\n\n%1").arg(QString::fromStdString(text)));
	}
}

