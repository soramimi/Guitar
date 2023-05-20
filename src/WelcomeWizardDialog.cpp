#include "WelcomeWizardDialog.h"
#include "ui_WelcomeWizardDialog.h"
#include <QFileDialog>
#include "MainWindow.h"
#include "common/misc.h"
#include "Git.h"
#include "UserEvent.h"

WelcomeWizardDialog::WelcomeWizardDialog(MainWindow *parent)
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

	global->avatar_loader.connectAvatarReady(this, &WelcomeWizardDialog::avatarReady);

	ui->stackedWidget->setCurrentWidget(ui->page_helper_tools);
}

WelcomeWizardDialog::~WelcomeWizardDialog()
{
	global->avatar_loader.disconnectAvatarReady(this, &WelcomeWizardDialog::avatarReady);
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
			Git g(gcx, {}, {}, {});
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

void WelcomeWizardDialog::setAvatar(QImage const &icon)
{
	ui->widget->setImage(icon);
}

void WelcomeWizardDialog::avatarReady()
{
	QString email = ui->lineEdit_user_email->text();
	auto icon = global->avatar_loader.fetch(email, true);
	setAvatar(icon);
}

void WelcomeWizardDialog::on_pushButton_get_icon_clicked()
{
	ui->widget->setImage({});
	QString email = ui->lineEdit_user_email->text();
	if (misc::isValidMailAddress(email)) {
		auto icon = global->avatar_loader.fetch(email, true);
		if (!icon.isNull()) {
			setAvatar(icon);
		}
	}
}

void WelcomeWizardDialog::on_lineEdit_git_textChanged(const QString &arg1)
{
	QString ss;
	if (!misc::isExecutable(arg1)) {
		ss = "* { background-color: #ffc0c0; }";
	}
	ui->lineEdit_git->setStyleSheet(ss);
}

