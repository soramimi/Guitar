#include "ConfigSigningDialog.h"
#include "SettingBehaviorForm.h"
#include "ui_SettingBehaviorForm.h"
#include "common/misc.h"
#include <QFileDialog>

SettingBehaviorForm::SettingBehaviorForm(QWidget *parent) :
	AbstractSettingForm(parent),
	ui(new Ui::SettingBehaviorForm)
{
	ui->setupUi(this);
}

SettingBehaviorForm::~SettingBehaviorForm()
{
	delete ui;
}

void SettingBehaviorForm::exchange(bool save)
{
	if (save) {
		settings()->automatically_fetch_when_opening_the_repository = ui->checkBox_auto_fetch->isChecked();
		settings()->get_committer_icon = ui->checkBox_get_committer_icon->isChecked();
		settings()->maximum_number_of_commit_item_acquisitions = (unsigned int)ui->spinBox_max_commit_item_acquisitions->value();
		settings()->default_working_dir = ui->lineEdit_default_working_dir->text();
	} else {
		ui->checkBox_auto_fetch->setChecked(settings()->automatically_fetch_when_opening_the_repository);
		ui->checkBox_get_committer_icon->setChecked(settings()->get_committer_icon);
		ui->spinBox_max_commit_item_acquisitions->setValue((int)settings()->maximum_number_of_commit_item_acquisitions);
		ui->lineEdit_default_working_dir->setText(settings()->default_working_dir);
	}
}

void SettingBehaviorForm::on_pushButton_signing_policy_clicked()
{
	ConfigSigningDialog dlg(this, mainwindow(), false);
	dlg.exec();
}

void SettingBehaviorForm::on_pushButton_browse_default_working_dir_clicked()
{
	QString dir = ui->lineEdit_default_working_dir->text();
	dir = QFileDialog::getExistingDirectory(this, tr("Default working folder"), dir);
	dir = misc::normalizePathSeparator(dir);
	if (QFileInfo(dir).isDir()) {
		settings()->default_working_dir = dir;
		ui->lineEdit_default_working_dir->setText(dir);
	}
}
