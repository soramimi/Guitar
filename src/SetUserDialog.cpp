#include "SetUserDialog.h"
#include "ui_SetUserDialog.h"
#include "common/misc.h"

SetUserDialog::SetUserDialog(QWidget *parent, Git::User global_user, Git::User repo_user, QString const &repo) :
	QDialog(parent),
	ui(new Ui::SetUserDialog)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	misc::setFixedSize(this);

	this->global_user = global_user;
	this->repo_user = repo_user;

	if (repo.isEmpty()) {
		ui->radioButton_repository->setEnabled(false);
	} else {
		QString text = tr("Repository");
		text += " : ";
		text += repo;
		ui->radioButton_repository->setText(text);
	}
	ui->radioButton_global->click();
	ui->lineEdit_name->setFocus();
}

SetUserDialog::~SetUserDialog()
{
	delete ui;
}

bool SetUserDialog::isGlobalChecked() const
{
	return ui->radioButton_global->isChecked();
}

bool SetUserDialog::isRepositoryChecked() const
{
	return ui->radioButton_repository->isChecked();
}

Git::User SetUserDialog::user() const
{
	Git::User user;
	user.name = ui->lineEdit_name->text();
	user.email = ui->lineEdit_mail->text();
	return user;
}

void SetUserDialog::on_radioButton_global_toggled(bool checked)
{
	if (checked) {
		ui->lineEdit_name->setText(global_user.name);
		ui->lineEdit_mail->setText(global_user.email);
	}
}

void SetUserDialog::on_radioButton_repository_toggled(bool checked)
{
	if (checked) {
		ui->lineEdit_name->setText(repo_user.name);
		ui->lineEdit_mail->setText(repo_user.email);
	}
}

void SetUserDialog::on_lineEdit_name_textChanged(const QString &text)
{
	if (isGlobalChecked()) {
		global_user.name = text;
	}
	if (isRepositoryChecked()) {
		repo_user.name = text;
	}
}

void SetUserDialog::on_lineEdit_mail_textChanged(const QString &text)
{
	if (isGlobalChecked()) {
		global_user.email = text;
	}
	if (isRepositoryChecked()) {
		repo_user.email = text;
	}
}
