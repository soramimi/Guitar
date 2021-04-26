
#include "SettingPrograms2Form.h"
#include "ui_SettingPrograms2Form.h"
#include <QFileDialog>
#include "SettingsDialog.h"
#include "common/misc.h"

SettingPrograms2Form::SettingPrograms2Form(QWidget *parent) :
	AbstractSettingForm(parent),
	ui(new Ui::SettingPrograms2Form)
{
	ui->setupUi(this);
}

SettingPrograms2Form::~SettingPrograms2Form()
{
	delete ui;
}

void SettingPrograms2Form::exchange(bool save)
{
	if (save) {
		settings()->terminal_command = ui->lineEdit_terminal_command->text();
		settings()->explorer_command = ui->lineEdit_explorer_command->text();
	} else {
		ui->lineEdit_terminal_command->setText(settings()->terminal_command);
		ui->lineEdit_explorer_command->setText(settings()->explorer_command);
	}
}

void SettingPrograms2Form::on_pushButton_select_terminal_command_clicked()
{
	QString path = QFileDialog::getOpenFileName(window(), tr("Terminal Command"), "/usr/bin");
	if (!path.isEmpty()) {
		settings()->terminal_command = path;
		ui->lineEdit_terminal_command->setText(path);
	}
}

void SettingPrograms2Form::on_pushButton_select_explorer_command_clicked()
{
	QString path = QFileDialog::getOpenFileName(window(), tr("Explorer Command"), "/usr/bin");
	if (!path.isEmpty()) {
		settings()->explorer_command = path;
		ui->lineEdit_explorer_command->setText(path);
	}
}

void SettingPrograms2Form::on_pushButton_reset_terminal_command_clicked()
{
	QString path = ApplicationSettings::defaultSettings().terminal_command;
	ui->lineEdit_terminal_command->setText(path);
}
