#include "SettingBehaviorForm.h"
#include "ui_SettingBehaviorForm.h"

SettingBehaviorForm::SettingBehaviorForm(QWidget *parent) :
	AbstractSettingForm(parent),
	ui(new Ui::SettingGeneralForm)
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
		settings()->automatically_fetch_when_opening_the_repository = ui->checkBox->isChecked();
	} else {
		ui->checkBox->setChecked(settings()->automatically_fetch_when_opening_the_repository);
	}
}
