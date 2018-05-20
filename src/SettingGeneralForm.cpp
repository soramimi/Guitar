#include "SettingGeneralForm.h"
#include "ui_SettingGeneralForm.h"

SettingGeneralForm::SettingGeneralForm(QWidget *parent) :
	AbstractSettingForm(parent),
	ui(new Ui::SettingGeneralForm)
{
	ui->setupUi(this);
	setWindowTitle(tr("General"));
}

SettingGeneralForm::~SettingGeneralForm()
{
	delete ui;
}

void SettingGeneralForm::exchange(bool save)
{
	if (save) {
		settings()->remember_and_restore_window_position = ui->checkBox_save_window_pos->isChecked();
		settings()->enable_high_dpi_scaling = ui->checkBox_enable_high_dpi_scaling->isChecked();
		if (ui->radioButton_theme_dark->isChecked()) {
			settings()->theme = "dark";
		} else {
			settings()->theme = "standard";
		}
	} else {
		ui->checkBox_save_window_pos->setChecked(settings()->remember_and_restore_window_position);
		ui->checkBox_enable_high_dpi_scaling->setChecked(settings()->enable_high_dpi_scaling);
		if (settings()->theme.compare("dark", Qt::CaseInsensitive) == 0) {
			ui->radioButton_theme_dark->click();
		} else {
			ui->radioButton_theme_standard->click();
		}
	}
}
