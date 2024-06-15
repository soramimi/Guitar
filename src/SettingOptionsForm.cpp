#include "SettingOptionsForm.h"
#include "ui_SettingOptionsForm.h"
#include "ApplicationGlobal.h"
#include "EditProfilesDialog.h"

SettingOptionsForm::SettingOptionsForm(QWidget *parent)
	: AbstractSettingForm(parent)
	, ui(new Ui::SettingOptionsForm)
{
	ui->setupUi(this);
	
	QStringList list;
	auto vec = ApplicationSettings::generative_ai_models();
	for (auto &m : vec) {
		list.push_back(m.model);
	}
	ui->comboBox_ai_model->addItems(list);
}

SettingOptionsForm::~SettingOptionsForm()
{
	delete ui;
}

void SettingOptionsForm::exchange(bool save)
{
	if (save) {
		settings()->generate_commit_message_by_ai = ui->groupBox_generate_commit_message_by_ai->isChecked();
		settings()->use_openai_api_key_environment_value = ui->checkBox_use_OPENAI_API_KEY_env_value->isChecked();
		settings()->use_anthropic_api_key_environment_value = ui->checkBox_use_ANTHROPIC_API_KEY_env_value->isChecked();
		settings()->openai_api_key = openai_api_key_;
		settings()->anthropic_api_key = anthropic_api_key_;
		settings()->ai_model = ui->comboBox_ai_model->currentText();
	} else {
		ui->groupBox_generate_commit_message_by_ai->setChecked(settings()->generate_commit_message_by_ai);
		ui->checkBox_use_OPENAI_API_KEY_env_value->setChecked(settings()->use_openai_api_key_environment_value);
		ui->checkBox_use_ANTHROPIC_API_KEY_env_value->setChecked(settings()->use_anthropic_api_key_environment_value);
		ui->lineEdit_openai_api_key->setText(settings()->openai_api_key);
		ui->lineEdit_anthropic_api_key->setText(settings()->anthropic_api_key);
		ui->comboBox_ai_model->setCurrentText(settings()->ai_model.model);

		openai_api_key_ = settings()->openai_api_key;
		anthropic_api_key_ = settings()->anthropic_api_key;
		ui->checkBox_use_OPENAI_API_KEY_env_value->setChecked(settings()->use_openai_api_key_environment_value);
		ui->checkBox_use_ANTHROPIC_API_KEY_env_value->setChecked(settings()->use_anthropic_api_key_environment_value);
		refrectSettingsToUI(true, true);
	}
}

void SettingOptionsForm::on_pushButton_edit_profiles_clicked()
{
	Git::User user = mainwindow()->currentGitUser();
	EditProfilesDialog dlg;
	dlg.loadXML(global->profiles_xml_path);
	dlg.enableDoubleClock(false);
	if (dlg.exec({user}) == QDialog::Accepted) {
		dlg.saveXML(global->profiles_xml_path);
	}
}

void SettingOptionsForm::refrectSettingsToUI(bool openai, bool anthropic)
{
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
}


void SettingOptionsForm::on_checkBox_use_OPENAI_API_KEY_env_value_stateChanged(int)
{
	refrectSettingsToUI(true, false);
}


void SettingOptionsForm::on_checkBox_use_ANTHROPIC_API_KEY_env_value_stateChanged(int)
{
	refrectSettingsToUI(false, true);
}


void SettingOptionsForm::on_lineEdit_openai_api_key_textChanged(const QString &arg1)
{
	if (!ui->checkBox_use_OPENAI_API_KEY_env_value->isChecked()) {
		openai_api_key_ = arg1;
	}
}


void SettingOptionsForm::on_lineEdit_anthropic_api_key_textChanged(const QString &arg1)
{
	if (!ui->checkBox_use_ANTHROPIC_API_KEY_env_value->isChecked()) {
		anthropic_api_key_ = arg1;
	}
}

