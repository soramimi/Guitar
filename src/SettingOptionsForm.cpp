#include "SettingOptionsForm.h"
#include "ui_SettingOptionsForm.h"
#include "ApplicationGlobal.h"
#include "EditProfilesDialog.h"
#include "GenerativeAI.h"
#include "IncrementalSearch.h"

#include <QMessageBox>

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
		settings()->incremental_search_with_miegemo = ui->checkBox_incremental_search_with_migemo->isChecked();
	} else {
		ui->checkBox_incremental_search_with_migemo->setChecked(settings()->incremental_search_with_miegemo);
	}
}

void SettingOptionsForm::on_pushButton_edit_profiles_clicked()
{
	GitUser user = mainwindow()->currentGitUser();
	EditProfilesDialog dlg;
	dlg.loadXML(global->profiles_xml_path);
	dlg.enableDoubleClock(false);
	if (dlg.exec({user}) == QDialog::Accepted) {
		dlg.saveXML(global->profiles_xml_path);
	}
}


void SettingOptionsForm::on_pushButton_setup_migemo_dict_clicked()
{
	auto r = QMessageBox::question(this, tr("Setup Migemo dictionary"), tr("Are you sure to setup Migemo dictionary?"), QMessageBox::Yes | QMessageBox::No);
	if (r == QMessageBox::Yes) {
		IncrementalSearch::setupMigemoDict();
		if (ui->checkBox_incremental_search_with_migemo->isChecked()) {
			global->incremental_search()->open();
		}
	}
}

void SettingOptionsForm::on_pushButton_delete_migemo_dict_clicked()
{
	auto r = QMessageBox::question(this, tr("Delete Migemo dictionary"), tr("Are you sure to delete Migemo dictionary?"), QMessageBox::Yes | QMessageBox::No);
	if (r == QMessageBox::Yes) {
		IncrementalSearch::deleteMigemoDict();
		global->incremental_search()->close();
	}
}


void SettingOptionsForm::on_checkBox_incremental_search_with_migemo_checkStateChanged(const Qt::CheckState &arg1)
{
	if (ui->checkBox_incremental_search_with_migemo->isChecked()) {
		global->incremental_search()->open();
	} else {
		global->incremental_search()->close();
	}
}

