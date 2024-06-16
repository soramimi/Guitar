#include "SettingAiForm.h"
#include "ui_SettingAiForm.h"
#include "ApplicationGlobal.h"
#include "EditProfilesDialog.h"
#include "GenerativeAI.h"

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
		settings()->use_openai_api_key_environment_value = ui->checkBox_use_OPENAI_API_KEY_env_value->isChecked();
		settings()->use_anthropic_api_key_environment_value = ui->checkBox_use_ANTHROPIC_API_KEY_env_value->isChecked();
		settings()->use_google_api_key_environment_value = ui->checkBox_use_GOOGLE_API_KEY_env_value->isChecked();
		settings()->openai_api_key = openai_api_key_;
		settings()->anthropic_api_key = anthropic_api_key_;
		settings()->google_api_key = google_api_key_;
		settings()->ai_model = ui->comboBox_ai_model->currentText();
	} else {
		ui->groupBox_generate_commit_message_by_ai->setChecked(settings()->generate_commit_message_by_ai);
		ui->checkBox_use_OPENAI_API_KEY_env_value->setChecked(settings()->use_openai_api_key_environment_value);
		ui->checkBox_use_ANTHROPIC_API_KEY_env_value->setChecked(settings()->use_anthropic_api_key_environment_value);
		ui->checkBox_use_GOOGLE_API_KEY_env_value->setChecked(settings()->use_google_api_key_environment_value);
		ui->lineEdit_openai_api_key->setText(settings()->openai_api_key);
		ui->lineEdit_anthropic_api_key->setText(settings()->anthropic_api_key);
		ui->lineEdit_google_api_key->setText(settings()->google_api_key);
		ui->comboBox_ai_model->setCurrentText(settings()->ai_model.name);

		openai_api_key_ = settings()->openai_api_key;
		anthropic_api_key_ = settings()->anthropic_api_key;
		google_api_key_ = settings()->google_api_key;
		ui->checkBox_use_OPENAI_API_KEY_env_value->setChecked(settings()->use_openai_api_key_environment_value);
		ui->checkBox_use_ANTHROPIC_API_KEY_env_value->setChecked(settings()->use_anthropic_api_key_environment_value);
		ui->checkBox_use_GOOGLE_API_KEY_env_value->setChecked(settings()->use_google_api_key_environment_value);
		refrectSettingsToUI(true, true);
	}
}

void SettingAiForm::refrectSettingsToUI_openai()
{
	QString apikey;
	if (ui->checkBox_use_OPENAI_API_KEY_env_value->isChecked()) {
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
	if (ui->checkBox_use_ANTHROPIC_API_KEY_env_value->isChecked()) {
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
	if (ui->checkBox_use_GOOGLE_API_KEY_env_value->isChecked()) {
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

void SettingAiForm::refrectSettingsToUI(bool openai, bool anthropic)
{
	refrectSettingsToUI_openai();
	refrectSettingsToUI_anthropic();
	refrectSettingsToUI_google();
}

void SettingAiForm::on_checkBox_use_OPENAI_API_KEY_env_value_stateChanged(int)
{
	refrectSettingsToUI_openai();
}


void SettingAiForm::on_checkBox_use_ANTHROPIC_API_KEY_env_value_stateChanged(int)
{
	refrectSettingsToUI_anthropic();
}

void SettingAiForm::on_checkBox_use_GOOGLE_API_KEY_env_value_checkStateChanged(const Qt::CheckState &arg1)
{
	refrectSettingsToUI_google();
}

void SettingAiForm::on_lineEdit_openai_api_key_textChanged(const QString &arg1)
{
	if (!ui->checkBox_use_OPENAI_API_KEY_env_value->isChecked()) {
		openai_api_key_ = arg1;
	}
}


void SettingAiForm::on_lineEdit_anthropic_api_key_textChanged(const QString &arg1)
{
	if (!ui->checkBox_use_ANTHROPIC_API_KEY_env_value->isChecked()) {
		anthropic_api_key_ = arg1;
	}
}

void SettingAiForm::on_lineEdit_google_api_key_textChanged(const QString &arg1)
{
	if (!ui->checkBox_use_GOOGLE_API_KEY_env_value->isChecked()) {
		google_api_key_ = arg1;
	}
}

