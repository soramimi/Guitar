#include "SettingAiForm.h"
#include "ui_SettingAiForm.h"
#include "GenerativeAI.h"

#include <QMessageBox>

SettingAiForm::SettingAiForm(QWidget *parent)
	: AbstractSettingForm(parent)
	, ui(new Ui::SettingAiForm)
{
	ui->setupUi(this);
	
	QStringList list;
	auto vec =GenerativeAI::available_models();
	for (auto &m : vec) {
		list.push_back(m.name);
	}
	ui->comboBox_ai_model->addItems(list);
}

SettingAiForm::~SettingAiForm()
{
	delete ui;
}

void SettingAiForm::exchange(bool save)
{
	if (save) {
		settings()->generate_commit_message_by_ai = ui->groupBox_generate_commit_message_by_ai->isChecked();
		settings()->use_openai_api_key_environment_value = ui->radioButton_use_OPENAI_API_KEY_env_value->isChecked();
		settings()->use_anthropic_api_key_environment_value = ui->radioButton_use_ANTHROPIC_API_KEY_env_value->isChecked();
		settings()->use_google_api_key_environment_value = ui->radioButton_use_GOOGLE_API_KEY_env_value->isChecked();
		settings()->openai_api_key = openai_api_key_;
		settings()->anthropic_api_key = anthropic_api_key_;
		settings()->google_api_key = google_api_key_;
		settings()->ai_model = ui->comboBox_ai_model->currentText();
	} else {
		ui->groupBox_generate_commit_message_by_ai->setChecked(settings()->generate_commit_message_by_ai);
		ui->radioButton_use_OPENAI_API_KEY_env_value->setChecked(settings()->use_openai_api_key_environment_value);
		ui->radioButton_use_ANTHROPIC_API_KEY_env_value->setChecked(settings()->use_anthropic_api_key_environment_value);
		ui->radioButton_use_GOOGLE_API_KEY_env_value->setChecked(settings()->use_google_api_key_environment_value);
		ui->lineEdit_openai_api_key->setText(settings()->openai_api_key);
		ui->lineEdit_anthropic_api_key->setText(settings()->anthropic_api_key);
		ui->lineEdit_google_api_key->setText(settings()->google_api_key);
		ui->comboBox_ai_model->setCurrentText(settings()->ai_model.name);

		openai_api_key_ = settings()->openai_api_key;
		anthropic_api_key_ = settings()->anthropic_api_key;
		google_api_key_ = settings()->google_api_key;
		// ui->radioButton_use_OPENAI_API_KEY_env_value->setChecked(settings()->use_openai_api_key_environment_value);
		// ui->radioButton_use_ANTHROPIC_API_KEY_env_value->setChecked(settings()->use_anthropic_api_key_environment_value);
		// ui->radioButton_use_GOOGLE_API_KEY_env_value->setChecked(settings()->use_google_api_key_environment_value);
		(settings()->use_openai_api_key_environment_value ? ui->radioButton_use_OPENAI_API_KEY_env_value
														  : ui->radioButton_use_custom_openai_api_key
															)->setChecked(true);
		(settings()->use_anthropic_api_key_environment_value ? ui->radioButton_use_ANTHROPIC_API_KEY_env_value
															 : ui->radioButton_use_custom_anthropic_api_key
															   )->setChecked(true);
		(settings()->use_google_api_key_environment_value ? ui->radioButton_use_GOOGLE_API_KEY_env_value
														  : ui->radioButton_use_custom_google_api_key
															)->setChecked(true);
		refrectSettingsToUI(true, true);
	}
}

void SettingAiForm::refrectSettingsToUI_openai()
{
	QString apikey;
	if (ui->radioButton_use_OPENAI_API_KEY_env_value->isChecked()) {
		apikey = getenv("OPENAI_API_KEY");
		ui->lineEdit_openai_api_key->setEnabled(false);
	} else {
		apikey = openai_api_key_;
		ui->lineEdit_openai_api_key->setEnabled(true);
	}
	bool b = ui->lineEdit_openai_api_key->blockSignals(true);
	ui->lineEdit_openai_api_key->setText(apikey);
	ui->lineEdit_openai_api_key->blockSignals(b);
}

void SettingAiForm::refrectSettingsToUI_anthropic()
{
	QString apikey;
	if (ui->radioButton_use_ANTHROPIC_API_KEY_env_value->isChecked()) {
		apikey = getenv("ANTHROPIC_API_KEY");
		ui->lineEdit_anthropic_api_key->setEnabled(false);
	} else {
		apikey = anthropic_api_key_;
		ui->lineEdit_anthropic_api_key->setEnabled(true);
	}
	bool b = ui->lineEdit_anthropic_api_key->blockSignals(true);
	ui->lineEdit_anthropic_api_key->setText(apikey);
	ui->lineEdit_anthropic_api_key->blockSignals(b);
}

void SettingAiForm::refrectSettingsToUI_google()
{
	QString apikey;
	if (ui->radioButton_use_GOOGLE_API_KEY_env_value->isChecked()) {
		apikey = getenv("GOOGLE_API_KEY");
		ui->lineEdit_google_api_key->setEnabled(false);
	} else {
		apikey = google_api_key_;
		ui->lineEdit_google_api_key->setEnabled(true);
	}
	bool b = ui->lineEdit_google_api_key->blockSignals(true);
	ui->lineEdit_google_api_key->setText(apikey);
	ui->lineEdit_google_api_key->blockSignals(b);
}

void SettingAiForm::refrectSettingsToUI_deepseek()
{
	QString apikey;
	if (ui->radioButton_use_DEEPSEEK_API_KEY_env_value->isChecked()) {
		apikey = getenv("DEEPSEEK_API_KEY");
		ui->lineEdit_deepseek_api_key->setEnabled(false);
	} else {
		apikey = deepseek_api_key_;
		ui->lineEdit_deepseek_api_key->setEnabled(true);
	}
	bool b = ui->lineEdit_deepseek_api_key->blockSignals(true);
	ui->lineEdit_deepseek_api_key->setText(apikey);
	ui->lineEdit_deepseek_api_key->blockSignals(b);
}

void SettingAiForm::refrectSettingsToUI(bool openai, bool anthropic)
{
	refrectSettingsToUI_openai();
	refrectSettingsToUI_anthropic();
	refrectSettingsToUI_google();
}

void SettingAiForm::on_lineEdit_openai_api_key_textChanged(const QString &arg1)
{
	if (!ui->radioButton_use_OPENAI_API_KEY_env_value->isChecked()) {
		openai_api_key_ = arg1;
	}
}

void SettingAiForm::on_lineEdit_anthropic_api_key_textChanged(const QString &arg1)
{
	if (!ui->radioButton_use_ANTHROPIC_API_KEY_env_value->isChecked()) {
		anthropic_api_key_ = arg1;
	}
}

void SettingAiForm::on_lineEdit_google_api_key_textChanged(const QString &arg1)
{
	if (!ui->radioButton_use_GOOGLE_API_KEY_env_value->isChecked()) {
		google_api_key_ = arg1;
	}
}

void SettingAiForm::on_lineEdit_deepseek_api_key_textChanged(const QString &arg1)
{
	if (!ui->radioButton_use_DEEPSEEK_API_KEY_env_value->isChecked()) {
		deepseek_api_key_ = arg1;
	}
}

void SettingAiForm::on_groupBox_generate_commit_message_by_ai_clicked(bool checked)
{
	if (checked) {
		QString msg = tr("ATTENTION");
		msg += "\n\n";
		// ja: AIを利用したコミットメッセージの生成機能を有効にすると、ローカルコンテンツの一部がクラウドサービスに送信されることに同意したものと見なされます。
		msg += tr("By enabling the commit message generation feature using AI, you are deemed to agree that part of your local content will be sent to the cloud service.");
		msg += "\n\n";
		// ja: あなたはAIモデルの選択、APIの利用料金、情報セキュリティに注意する必要があります。
		msg += tr("You should be aware of AI model selection, API usage fees, and information security.");
		if (QMessageBox::warning(this, tr("Commit Message Generation with AI"), msg, QMessageBox::Ok, QMessageBox::Cancel) != QMessageBox::Ok)	{
			ui->groupBox_generate_commit_message_by_ai->setChecked(false);
		}
	}
}

void SettingAiForm::on_radioButton_use_OPENAI_API_KEY_env_value_clicked()
{
	refrectSettingsToUI_openai();
}

void SettingAiForm::on_radioButton_use_custom_openai_api_key_clicked()
{
	refrectSettingsToUI_openai();
}

void SettingAiForm::on_radioButton_use_ANTHROPIC_API_KEY_env_value_clicked()
{
	refrectSettingsToUI_anthropic();
}

void SettingAiForm::on_radioButton_use_custom_anthropic_api_key_clicked()
{
	refrectSettingsToUI_anthropic();
}

void SettingAiForm::on_radioButton_use_GOOGLE_API_KEY_env_value_clicked()
{
	refrectSettingsToUI_google();
}

void SettingAiForm::on_radioButton_use_custom_google_api_key_clicked()
{
	refrectSettingsToUI_google();
}

void SettingAiForm::on_radioButton_use_DEEPSEEK_API_KEY_env_value_clicked()
{
	refrectSettingsToUI_deepseek();
}

void SettingAiForm::on_radioButton_use_custom_deepseek_api_key_clicked()
{
	refrectSettingsToUI_deepseek();
}

void SettingAiForm::on_comboBox_ai_model_currentTextChanged(const QString &arg1)
{
	GenerativeAI::Model model(arg1);
	GenerativeAI::Type type = model.type();
	if (model_.type() != type) {
		switch (type) {
		case GenerativeAI::GPT:
			ui->stackedWidget->setCurrentWidget(ui->page_openai);
			break;
		case GenerativeAI::CLAUDE:
			ui->stackedWidget->setCurrentWidget(ui->page_anthropic);
			break;
		case GenerativeAI::GEMINI:
			ui->stackedWidget->setCurrentWidget(ui->page_google);
			break;
		case GenerativeAI::DEEPSEEK:
			ui->stackedWidget->setCurrentWidget(ui->page_deepseek);
			break;
		default:
			ui->stackedWidget->setCurrentWidget(ui->page_unknown);
			break;
		}
		model_ = model;
	}
}

