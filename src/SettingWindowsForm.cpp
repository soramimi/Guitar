#include "SettingWindowsForm.h"
#include "ui_SettingWindowsForm.h"

SettingWindowsForm::SettingWindowsForm(QWidget *parent)
	: AbstractSettingForm(parent)
	, ui(new Ui::SettingWindowsForm)
{
	ui->setupUi(this);
}

SettingWindowsForm::~SettingWindowsForm()
{
	delete ui;
}

void SettingWindowsForm::exchange(bool save)
{
	if (save) {
	} else {
	}
}
