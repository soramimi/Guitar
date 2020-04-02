
#include "SettingProgramsForm.h"
#include "ui_SettingProgramsForm.h"
#include <QFileDialog>
#include "SettingsDialog.h"
#include "common/misc.h"

SettingProgramsForm::SettingProgramsForm(QWidget *parent) :
	AbstractSettingForm(parent),
	ui(new Ui::SettingProgramsForm)
{
	ui->setupUi(this);
}

SettingProgramsForm::~SettingProgramsForm()
{
	delete ui;
}

void SettingProgramsForm::exchange(bool save)
{
	if (save) {
		settings()->git_command = ui->lineEdit_git_command->text();
		settings()->file_command = ui->lineEdit_file_command->text();
		settings()->gpg_command = ui->lineEdit_gpg_command->text();
		settings()->ssh_command = ui->lineEdit_ssh_command->text();
	} else {
		ui->lineEdit_git_command->setText(settings()->git_command);
		ui->lineEdit_file_command->setText(settings()->file_command);
		ui->lineEdit_gpg_command->setText(settings()->gpg_command);
		ui->lineEdit_ssh_command->setText(settings()->ssh_command);
	}
}

void SettingProgramsForm::on_pushButton_select_git_command_clicked()
{
	QString path = mainwindow()->selectGitCommand(false);
	if (!path.isEmpty()) {
		settings()->git_command = path;
		ui->lineEdit_git_command->setText(path);
	}
}

void SettingProgramsForm::on_pushButton_select_file_command_clicked()
{
	QString path = mainwindow()->selectFileCommand(false);
	if (!path.isEmpty()) {
		settings()->file_command = path;
		ui->lineEdit_file_command->setText(path);
	}
}

void SettingProgramsForm::on_pushButton_select_gpg_command_clicked()
{
	QString path = mainwindow()->selectGpgCommand(false);
	if (!path.isEmpty()) {
		settings()->gpg_command = path;
		ui->lineEdit_gpg_command->setText(path);
	}
}

void SettingProgramsForm::on_pushButton_select_ssh_command_clicked()
{
	QString path = mainwindow()->selectSshCommand(false);
	if (!path.isEmpty()) {
		settings()->ssh_command = path;
		ui->lineEdit_ssh_command->setText(path);
	}
}
