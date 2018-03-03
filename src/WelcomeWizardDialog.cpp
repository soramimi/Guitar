#include "WelcomeWizardDialog.h"
#include "ui_WelcomeWizardDialog.h"
#include <QFileDialog>
#include "MainWindow.h"
#include "common/misc.h"

WelcomeWizardDialog::WelcomeWizardDialog(MainWindow *parent) :
	QDialog(parent),
	ui(new Ui::WelcomeWizardDialog)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	mainwindow_ = parent;

	avatar_loader_.start(mainwindow_->webContext());
	connect(&avatar_loader_, &AvatarLoader::updated, [&](){
		QString email = ui->lineEdit_user_email->text();
		QIcon icon = avatar_loader_.fetch(email.toStdString(), false);
		ui->label_avatar->setPixmap(icon.pixmap(QSize(64, 64)));
	});

	ui->stackedWidget->setCurrentWidget(ui->page_global_user_information);
	ui->lineEdit_user_name->setFocus();
}

WelcomeWizardDialog::~WelcomeWizardDialog()
{
	delete ui;
	avatar_loader_.interrupt();
	avatar_loader_.wait();
}

void WelcomeWizardDialog::set_user_name(const QString &v)
{
	ui->lineEdit_user_name->setText(v);
}

void WelcomeWizardDialog::set_user_email(const QString &v)
{
	ui->lineEdit_user_email->setText(v);
}

void WelcomeWizardDialog::set_default_working_folder(const QString &v)
{
	ui->lineEdit_default_working_folder->setText(v);

}

void WelcomeWizardDialog::set_git_command_path(const QString &v)
{
	ui->lineEdit_git->setText(v);
}

void WelcomeWizardDialog::set_file_command_path(const QString &v)
{
	ui->lineEdit_file->setText(v);
}

QString WelcomeWizardDialog::user_name() const
{
	return ui->lineEdit_user_name->text();
}

QString WelcomeWizardDialog::user_email() const
{
	return ui->lineEdit_user_email->text();
}

QString WelcomeWizardDialog::default_working_folder() const
{
	return ui->lineEdit_default_working_folder->text();
}

QString WelcomeWizardDialog::git_command_path() const
{
	return ui->lineEdit_git->text();
}

QString WelcomeWizardDialog::file_command_path() const
{
	return ui->lineEdit_file->text();
}

void WelcomeWizardDialog::on_pushButton_to_default_working_folder_clicked()
{
	ui->stackedWidget->setCurrentWidget(ui->page_default_working_folder);
}

void WelcomeWizardDialog::on_pushButton_to_global_user_information_clicked()
{
	ui->stackedWidget->setCurrentWidget(ui->page_global_user_information);
}

void WelcomeWizardDialog::on_pushButton_to_helper_tools_clicked()
{
	ui->stackedWidget->setCurrentWidget(ui->page_helper_tools);
}

void WelcomeWizardDialog::on_pushButton_to_default_working_folder_2_clicked()
{
	ui->stackedWidget->setCurrentWidget(ui->page_default_working_folder);
}

void WelcomeWizardDialog::on_pushButton_to_finish_clicked()
{
	ui->stackedWidget->setCurrentWidget(ui->page_finish);

}

void WelcomeWizardDialog::on_pushButton_to_helper_tools_2_clicked()
{
	ui->stackedWidget->setCurrentWidget(ui->page_helper_tools);
}

void WelcomeWizardDialog::on_stackedWidget_currentChanged(int /*arg1*/)
{
	QWidget *w = ui->stackedWidget->currentWidget();
	if (w == ui->page_global_user_information) {
		ui->pushButton_to_default_working_folder->setDefault(true);
		ui->lineEdit_user_name->setFocus();
	} else if (w == ui->page_default_working_folder) {
		ui->pushButton_to_helper_tools->setDefault(true);
		ui->lineEdit_default_working_folder->setFocus();
	} else if (w == ui->page_helper_tools) {
		ui->pushButton_to_finish->setDefault(true);
		ui->lineEdit_git->setFocus();
	} else if (w == ui->page_finish) {
		ui->lineEdit_preview_user->setText(ui->lineEdit_user_name->text());
		ui->lineEdit_preview_email->setText(ui->lineEdit_user_email->text());
		ui->lineEdit_preview_folder->setText(ui->lineEdit_default_working_folder->text());
		ui->lineEdit_preview_git->setText(ui->lineEdit_git->text());
		ui->lineEdit_preview_file->setText(ui->lineEdit_file->text());
	}
}

void WelcomeWizardDialog::on_pushButton_browse_default_workiing_folder_clicked()
{
	QString s = ui->lineEdit_default_working_folder->text();
	s = QFileDialog::getExistingDirectory(this, tr("Default Working Folder"), s);
	s = misc::normalizePathSeparator(s);
	ui->lineEdit_default_working_folder->setText(s);
}


void WelcomeWizardDialog::on_pushButton_browse_git_clicked()
{
	QString s = mainwindow_->selectGitCommand(false);
	ui->lineEdit_git->setText(s);
}

void WelcomeWizardDialog::on_pushButton_browse_file_clicked()
{
	QString s = mainwindow_->selectFileCommand(false);
	ui->lineEdit_file->setText(s);
}

void WelcomeWizardDialog::on_pushButton_get_icon_clicked()
{
	QString email = ui->lineEdit_user_email->text();
	if (email.indexOf('@') > 0) {
		avatar_loader_.fetch(email.toStdString(), true);
	}
}
