#include "SelectAiModelDialog.h"
#include "ui_SelectAiModelDialog.h"

#include "SelectAiModelPresetDialog.h"
#include <ai/AiApiBridge.h>
#include <ai/GenerativeAI.h>
#include <QListWidgetItem>

using namespace GenerativeAI;

namespace {

enum {
	ModelIndexRole = Qt::UserRole,
};

}

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

	ui->splitter->setSizes({100, 300});
	
	for (ProviderInfo const &provider : complete_provider_table()) {
		if (provider.tag.empty()) continue; // Skip placeholder entries
		ui->comboBox_provider->addItem(QString::fromStdString(provider.description), (int)provider.id);
	}
	
	static constexpr std::string_view api_openai_chat_completions_v1 = "openai_chat_completions_v1";
	static constexpr std::string_view api_openai_responses_v1 = "openai_responses_v1";
	static constexpr std::string_view api_anthropic_messages_v1 = "anthropic_messages_v1";
	static constexpr std::string_view api_google_gemini_v1 = "google_gemini_v1";
	
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
	delete m;
	delete ui;
}



void SelectAiModelDialog::on_pushButton_load_preset_clicked()
{
	SelectAiModelPresetDialog dlg(this);
	if (dlg.exec() == QDialog::Accepted) {
		m->model = dlg.selectedModel();
		Model const &model = m->model;
		
		ui->comboBox_provider->setCurrentIndex(ui->comboBox_provider->findData((int)model.provider_id()));
		
		ui->comboBox_api_type->setCurrentIndex(ui->comboBox_api_type->findData((int)model.api_compatibility()));
		
		Request req = make_request(model.provider_id(), model, {});
		ui->lineEdit_endpoint_url->setText(QString::fromStdString(req.endpoint.url_chat()));
	}
}

void SelectAiModelDialog::on_pushButton_fetch_model_clicked()
{
	ui->comboBox_model->clear();
	
	m->model.provider_info_ = provider_info(m->model.provider_id());
	m->model.endpoint_url_override = ui->lineEdit_endpoint_url->text().toStdString();
	
	AiApiBridge api;
	api.set_ai_model(m->model);
	std::optional<AiResult::Models> models = api.queryModels();
	if (models == std::nullopt) return;
	
	std::sort(models->list.begin(), models->list.end(), [](AiResult::Model const &a, AiResult::Model const &b) {
		return a.id < b.id;
	});
	
	for (size_t i = 0; i < models->list.size(); i++) {
		AiResult::Model const &model = models->list[i];
		ui->comboBox_model->addItem(QString::fromStdString(model.id), (int)i);
	}
}

void SelectAiModelDialog::on_comboBox_provider_currentIndexChanged(int index)
{
	if (index >= 0 && index < ui->comboBox_provider->count()) {
		int i = ui->comboBox_provider->itemData(index).toInt();
		ProviderInfo const &provider = complete_provider_table()[i];
		m->model.provider_info_ = &provider;
		ui->comboBox_api_type->setCurrentIndex(ui->comboBox_api_type->findData((int)m->model.api_compatibility()));
	}
}

void SelectAiModelDialog::on_comboBox_api_type_currentIndexChanged(int index)
{
	if (index >= 0 && index < ui->comboBox_api_type->count()) {
		ProviderID api_type_id = (ProviderID)ui->comboBox_api_type->itemData(index).toInt();
		m->model.api_compatibility_override = api_type_id;
		
		Request req = make_request(m->model.provider_id(), m->model, {});
		ui->lineEdit_endpoint_url->setText(QString::fromStdString(req.endpoint.url_chat()));
	}
}




