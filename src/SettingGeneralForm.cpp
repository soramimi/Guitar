#include "SettingGeneralForm.h"
#include "ui_SettingGeneralForm.h"
#include "common/misc.h"

#include <QFileDialog>

SettingGeneralForm::SettingGeneralForm(QWidget *parent) :
	AbstractSettingForm(parent),
	ui(new Ui::SettingGeneralForm)
{
	ui->setupUi(this);
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
		settings()->default_working_dir = ui->lineEdit_default_working_dir->text();
		if (ui->radioButton_theme_dark->isChecked()) {
			settings()->theme = "dark";
		} else {
			settings()->theme = "standard";
		}
	} else {
		ui->checkBox_save_window_pos->setChecked(settings()->remember_and_restore_window_position);
		ui->checkBox_enable_high_dpi_scaling->setChecked(settings()->enable_high_dpi_scaling);
		ui->lineEdit_default_working_dir->setText(settings()->default_working_dir);
		if (settings()->theme.compare("dark", Qt::CaseInsensitive) == 0) {
			ui->radioButton_theme_dark->click();
		} else {
			ui->radioButton_theme_standard->click();
		}
	}
}

void SettingGeneralForm::on_pushButton_browse_default_working_dir_clicked()
{
	QString dir = ui->lineEdit_default_working_dir->text();
	dir = QFileDialog::getExistingDirectory(this, tr("Default working folder"), dir);
	dir = misc::normalizePathSeparator(dir);
	if (QFileInfo(dir).isDir()) {
		settings()->default_working_dir = dir;
		ui->lineEdit_default_working_dir->setText(dir);
	}
}
