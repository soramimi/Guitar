#include "ConfigSigningDialog.h"
#include "SettingBehaviorForm.h"
#include "ui_SettingBehaviorForm.h"

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
		settings()->maximum_number_of_commit_item_acquisitions = ui->spinBox_max_commit_item_acquisitions->value();
	} else {
		ui->checkBox_auto_fetch->setChecked(settings()->automatically_fetch_when_opening_the_repository);
		ui->checkBox_get_committer_icon->setChecked(settings()->get_committer_icon);
		ui->spinBox_max_commit_item_acquisitions->setValue(settings()->maximum_number_of_commit_item_acquisitions);
	}
}

void SettingBehaviorForm::on_pushButton_signing_policy_clicked()
{
	ConfigSigningDialog dlg(this, mainwindow(), false);
	dlg.exec();
}
