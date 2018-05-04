#include "ConfigSigningDialog.h"
#include "SettingBehaviorForm.h"
#include "ui_SettingBehaviorForm.h"

SettingBehaviorForm::SettingBehaviorForm(QWidget *parent) :
	AbstractSettingForm(parent),
	ui(new Ui::SettingBehaviorForm)
{
	ui->setupUi(this);
	setWindowTitle(tr("Behavior"));
}

SettingBehaviorForm::~SettingBehaviorForm()
{
	delete ui;
}

void SettingBehaviorForm::exchange(bool save)
{
	if (save) {
		settings()->automatically_fetch_when_opening_the_repository = ui->checkBox_auto_fetch->isChecked();
	} else {
		ui->checkBox_auto_fetch->setChecked(settings()->automatically_fetch_when_opening_the_repository);
	}
}

void SettingBehaviorForm::on_pushButton_signing_policy_clicked()
{
	ConfigSigningDialog dlg(this, mainwindow(), false);
	dlg.exec();
}
