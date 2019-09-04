#include "WelcomeWizardDialog.h"
#include "ui_WelcomeWizardDialog.h"
#include <QFileDialog>
#include "MainWindow.h"
#include "common/misc.h"
#include "Git.h"

WelcomeWizardDialog::WelcomeWizardDialog(BasicMainWindow *parent)
	: QDialog(parent)
	, ui(new Ui::WelcomeWizardDialog)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	mainwindow_ = parent;

	pages_.push_back(ui->page_helper_tools);
	pages_.push_back(ui->page_global_user_information);
	pages_.push_back(ui->page_default_working_folder);
	pages_.push_back(ui->page_finish);
	ui->stackedWidget->setCurrentWidget(pages_[0]);
	on_stackedWidget_currentChanged(0);

	avatar_loader_.start(mainwindow_);
	connect(&avatar_loader_, &AvatarLoader::updated, [&](){
		QString email = ui->lineEdit_user_email->text();
		QIcon icon = avatar_loader_.fetch(email.toStdString(), false);
		setAvatar(icon);
	});

	ui->stackedWidget->setCurrentWidget(ui->page_helper_tools);
}

WelcomeWizardDialog::~WelcomeWizardDialog()
{
	avatar_loader_.stop();
	delete ui;
}

void WelcomeWizardDialog::set_user_name(QString const &v)
{
	ui->lineEdit_user_name->setText(v);
}

void WelcomeWizardDialog::set_user_email(QString const &v)
{
	ui->lineEdit_user_email->setText(v);
}

void WelcomeWizardDialog::set_default_working_folder(QString const &v)
{
	ui->lineEdit_default_working_folder->setText(v);

}

void WelcomeWizardDialog::set_git_command_path(QString const &v)
{
	ui->lineEdit_git->setText(v);
}

void WelcomeWizardDialog::set_file_command_path(QString const &v)
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

void WelcomeWizardDialog::on_pushButton_prev_clicked()
{
	int i = pages_.indexOf(ui->stackedWidget->currentWidget());
	if (i == 0) {
		done(QDialog::Rejected);
		return;
	}
	if (i > 0) {
		i--;
		QWidget *w = pages_[i];
		ui->stackedWidget->setCurrentWidget(w);
	}
}

void WelcomeWizardDialog::on_pushButton_next_clicked()
{
	if (ui->stackedWidget->currentWidget() == ui->page_finish) {
		done(QDialog::Accepted);
	}
	int i = pages_.indexOf(ui->stackedWidget->currentWidget());
	if (i + 1 < pages_.size()) {
		i++;
		QWidget *w = pages_[i];
		ui->stackedWidget->setCurrentWidget(w);
	}
}

void WelcomeWizardDialog::on_stackedWidget_currentChanged(int /*arg1*/)
{
	QString prev_text;
	QString next_text;
	QWidget *w = ui->stackedWidget->currentWidget();
	if (w == ui->page_helper_tools) {
		prev_text = tr("Cancel");
		ui->lineEdit_git->setFocus();
	} else if (w == ui->page_global_user_information) {
		if (user_name().isEmpty() && user_email().isEmpty()) {
			Git::Context gcx;
			gcx.git_command = git_command_path();
			Git g(gcx, QString());
			Git::User user = g.getUser(Git::Source::Global);
			set_user_name(user.name);
			set_user_email(user.email);
		}
		if (user_name().isEmpty()) {
			ui->lineEdit_user_name->setFocus();
		} else if (user_email().isEmpty()) {
			ui->lineEdit_user_email->setFocus();
		} else {
			ui->pushButton_next->setFocus();
		}
	} else if (w == ui->page_default_working_folder) {
		ui->lineEdit_default_working_folder->setFocus();
	} else if (w == ui->page_finish) {
		ui->lineEdit_preview_user->setText(ui->lineEdit_user_name->text());
		ui->lineEdit_preview_email->setText(ui->lineEdit_user_email->text());
		ui->lineEdit_preview_folder->setText(ui->lineEdit_default_working_folder->text());
		ui->lineEdit_preview_git->setText(ui->lineEdit_git->text());
		ui->lineEdit_preview_file->setText(ui->lineEdit_file->text());
		next_text = tr("Finish");
		ui->pushButton_next->setFocus();
	}
	ui->pushButton_prev->setText(prev_text.isEmpty() ? tr("<< Prev") : prev_text);
	ui->pushButton_next->setText(next_text.isEmpty() ? tr("Next >>") : next_text);
	ui->pushButton_next->setDefault(true);
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

void WelcomeWizardDialog::setAvatar(QIcon const &icon)
{
	QPixmap pm = icon.pixmap(QSize(64, 64));
	ui->label_avatar->setPixmap(pm);
}

void WelcomeWizardDialog::on_pushButton_get_icon_clicked()
{
	ui->label_avatar->setPixmap(QPixmap());
	QString email = ui->lineEdit_user_email->text();
	if (email.indexOf('@') > 0) {
		QIcon icon = avatar_loader_.fetch(email.toStdString(), true);
		if (!icon.isNull()) {
			setAvatar(icon);
		}
	}
}

