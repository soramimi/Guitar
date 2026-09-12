#include "SelectAiModelDialog.h"
#include "ui_SelectAiModelDialog.h"

#include "SelectAiModelPresetDialog.h"
#include <ai/AiApiBridge.h>
#include <ai/GenerativeAI.h>

using namespace GenerativeAI;

SelectAiModelDialog::SelectAiModelDialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::SelectAiModelDialog)
{
	ui->setupUi(this);

	ui->splitter->setSizes({100, 300});
	
	std::vector<ProviderInfo> const &providers = complete_provider_table();
	for (ProviderInfo const &provider : providers) {
		if (provider.tag.empty()) continue; // Skip placeholder entries
		ui->comboBox_provider->addItem(QString::fromStdString(provider.description), QString::fromStdString(provider.tag));
	}
	
	static constexpr std::string_view api_openai_chat_completions_v1 = "openai_chat_completions_v1";
	static constexpr std::string_view api_openai_responses_v1 = "openai_responses_v1";
	static constexpr std::string_view api_anthropic_messages_v1 = "anthropic_messages_v1";
	static constexpr std::string_view api_google_gemini_v1 = "google_gemini_v1";
	
	auto AddApiType = [this](const std::string_view& api_type) {
		ui->comboBox_api_type->addItem(QString::fromStdString(api_type.data()), QString::fromStdString(api_type.data()));
	};
	AddApiType(api_openai_chat_completions_v1);
	AddApiType(api_openai_responses_v1);
	AddApiType(api_anthropic_messages_v1);
	AddApiType(api_google_gemini_v1);
}

SelectAiModelDialog::~SelectAiModelDialog()
{
	delete ui;
}

void SelectAiModelDialog::on_pushButton_load_preset_clicked()
{
	SelectAiModelPresetDialog dlg(this);
	dlg.exec();
}



void SelectAiModelDialog::on_pushButton_fetch_model_clicked()
{
	AiApiBridge api;
	std::optional<AiResult::Models> models = api.queryModels();
	if (models == std::nullopt) return;
	
	for (AiResult::Model const &model : models->list) {
		// ui->listWidget_models->addItem(QString::fromStdString(model.model_uri.string));
		qDebug() << model.id.c_str();
	}
}

