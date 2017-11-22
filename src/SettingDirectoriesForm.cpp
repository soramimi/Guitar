
#include "SettingDirectoriesForm.h"
#include "ui_SettingDirectoriesForm.h"
#include <QFileDialog>
#include "SettingsDialog.h"
#include "common/misc.h"

SettingDirectoriesForm::SettingDirectoriesForm(QWidget *parent) :
	AbstractSettingForm(parent),
	ui(new Ui::SettingDirectoriesForm)
{
	ui->setupUi(this);
}

SettingDirectoriesForm::~SettingDirectoriesForm()
{
	delete ui;
}

void SettingDirectoriesForm::exchange(bool save)
{
	if (save) {
		settings()->default_working_dir = ui->lineEdit_default_working_dir->text();
		settings()->git_command = ui->lineEdit_git_command->text();
		settings()->file_command = ui->lineEdit_file_command->text();
	} else {
		ui->lineEdit_default_working_dir->setText(settings()->default_working_dir);
		ui->lineEdit_git_command->setText(settings()->git_command);
		ui->lineEdit_file_command->setText(settings()->file_command);
	}
}

void SettingDirectoriesForm::on_pushButton_select_git_command_clicked()
{
	QString path = mainwindow()->selectGitCommand(false);
	if (!path.isEmpty()) {
		settings()->git_command = path;
		ui->lineEdit_git_command->setText(path);
	}
}

void SettingDirectoriesForm::on_pushButton_select_file_command_clicked()
{
	QString path = mainwindow()->selectFileCommand();
	if (!path.isEmpty()) {
		settings()->file_command = path;
		ui->lineEdit_file_command->setText(path);
	}
}

void SettingDirectoriesForm::on_pushButton_browse_default_working_dir_clicked()
{
	QString dir = ui->lineEdit_default_working_dir->text();
	dir = QFileDialog::getExistingDirectory(this, tr("Default working folder"), dir);
	dir = misc::normalizePathSeparator(dir);
	if (QFileInfo(dir).isDir()) {
		settings()->default_working_dir = dir;
		ui->lineEdit_default_working_dir->setText(dir);
	}
}


