#include "SettingsDialog.h"
#include "ui_SettingsDialog.h"
#include "MySettings.h"

SettingsDialog::SettingsDialog(MainWindow *parent) :
	QDialog(parent),
	ui(new Ui::SettingsDialog)
{
	ui->setupUi(this);

	mainwindow = parent;

	loadSettings();
}

SettingsDialog::~SettingsDialog()
{
	delete ui;
}

void SettingsDialog::loadSettings(ApplicationSettings *set)
{
	MySettings s;
	s.beginGroup("Global");
	set->default_working_dir = s.value("DefaultWorkingDirectory").toString();
	set->git_command = s.value("GitCommand").toString();
	set->file_command = s.value("FileCommand").toString();
	s.endGroup();
}

void SettingsDialog::saveSettings()
{
	set.default_working_dir = ui->lineEdit_default_working_dir->text();

	MySettings s;
	s.beginGroup("Global");
	s.setValue("DefaultWorkingDirectory", set.default_working_dir);
	s.setValue("GitCommand", set.git_command);
	s.setValue("FileCommand", set.file_command);
	s.endGroup();
}

void SettingsDialog::loadSettings()
{
	loadSettings(&set);

	ui->lineEdit_default_working_dir->setText(set.default_working_dir);
	ui->lineEdit_git_command->setText(set.git_command);
	ui->lineEdit_file_command->setText(set.file_command);
}

void SettingsDialog::on_pushButton_select_git_command_clicked()
{
	QString path = mainwindow->selectGitCommand();
	if (!path.isEmpty()) {
		set.git_command = path;
		ui->lineEdit_git_command->setText(path);
	}
}

void SettingsDialog::accept()
{
	saveSettings();
	QDialog::accept();
}

void SettingsDialog::on_pushButton_select_file_command_clicked()
{
	QString path = mainwindow->selectFileCommand();
	if (!path.isEmpty()) {
		set.file_command = path;
		ui->lineEdit_file_command->setText(path);
	}
}
