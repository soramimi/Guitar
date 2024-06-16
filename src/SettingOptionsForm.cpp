#include "SettingOptionsForm.h"
#include "ui_SettingOptionsForm.h"
#include "ApplicationGlobal.h"
#include "EditProfilesDialog.h"
#include "GenerativeAI.h"

SettingOptionsForm::SettingOptionsForm(QWidget *parent)
	: AbstractSettingForm(parent)
	, ui(new Ui::SettingOptionsForm)
{
	ui->setupUi(this);
}

SettingOptionsForm::~SettingOptionsForm()
{
	delete ui;
}

void SettingOptionsForm::exchange(bool save)
{
	if (save) {
	} else {
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

