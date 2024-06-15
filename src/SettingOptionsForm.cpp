#include "SettingOptionsForm.h"
#include "ui_SettingOptionsForm.h"
#include "ApplicationGlobal.h"
#include "EditProfilesDialog.h"

SettingOptionsForm::SettingOptionsForm(QWidget *parent)
	: AbstractSettingForm(parent)
	, ui(new Ui::SettingOptionsForm)
{
	ui->setupUi(this);
	
	QStringList list = ApplicationSettings::openai_gpt_models();
	ui->comboBox_openai_gpt_model->addItems(list);
}

SettingOptionsForm::~SettingOptionsForm()
{
	delete ui;
}

void SettingOptionsForm::exchange(bool save)
{
	if (save) {
		settings()->generate_commit_message_by_ai = ui->groupBox_generate_commit_message_by_ai->isChecked();
		settings()->use_OPENAI_API_KEY_env_value = ui->checkBox_use_OPENAI_API_KEY_env_value->isChecked();
		settings()->openai_api_key_by_aicommits = ui->lineEdit_openai_api_key->text();
		settings()->openai_gpt_model = ui->comboBox_openai_gpt_model->currentText();
	} else {
		ui->groupBox_generate_commit_message_by_ai->setChecked(settings()->generate_commit_message_by_ai);
		ui->checkBox_use_OPENAI_API_KEY_env_value->setChecked(settings()->use_OPENAI_API_KEY_env_value);
		ui->lineEdit_openai_api_key->setText(settings()->openai_api_key_by_aicommits);
		ui->comboBox_openai_gpt_model->setCurrentText(settings()->openai_gpt_model);

		ui->checkBox_use_OPENAI_API_KEY_env_value->setChecked(settings()->use_OPENAI_API_KEY_env_value);
		refrectSettingsToUI();
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

void SettingOptionsForm::refrectSettingsToUI()
{
	QString apikey;
	if (ui->checkBox_use_OPENAI_API_KEY_env_value->isChecked()) {
		apikey = getenv("OPENAI_API_KEY");
		ui->lineEdit_openai_api_key->setText(apikey);
	} else {
		apikey = settings()->openai_api_key_by_aicommits;
	}
	ui->lineEdit_openai_api_key->setText(apikey);
}


void SettingOptionsForm::on_checkBox_use_OPENAI_API_KEY_env_value_stateChanged(int)
{
	refrectSettingsToUI();
}

