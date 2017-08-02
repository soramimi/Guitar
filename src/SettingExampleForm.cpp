#include "SettingExampleForm.h"
#include "ui_SettingExampleForm.h"

SettingExampleForm::SettingExampleForm(QWidget *parent) :
	AbstractSettingForm(parent),
	ui(new Ui::SettingExampleForm)
{
	ui->setupUi(this);
}

SettingExampleForm::~SettingExampleForm()
{
	delete ui;
}

void SettingExampleForm::exchange(bool save)
{
	if (save) {
	} else {
	}
}
