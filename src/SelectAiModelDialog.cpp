#include "SelectAiModelDialog.h"
#include "SelectAiModelPresetDialog.h"
#include "ui_SelectAiModelDialog.h"

SelectAiModelDialog::SelectAiModelDialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::SelectAiModelDialog)
{
	ui->setupUi(this);

	static constexpr std::string_view api_openai_chat_completions_v1 = "openai_chat_completions_v1";
	static constexpr std::string_view api_openai_responses_v1 = "openai_responses_v1";
	static constexpr std::string_view api_anthropic_messages_v1 = "anthropic_messages_v1";
	
	auto AddApiType = [this](const std::string_view& api_type) {
		ui->comboBox_api_type->addItem(QString::fromStdString(api_type.data()), QString::fromStdString(api_type.data()));
	};
	AddApiType(api_openai_chat_completions_v1);
	AddApiType(api_openai_responses_v1);
	AddApiType(api_anthropic_messages_v1);
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

