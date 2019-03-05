#include "SetUserDialog.h"
#include "ui_SetUserDialog.h"
#include "AvatarLoader.h"
#include "BasicMainWindow.h"
#include "common/misc.h"

struct SetUserDialog::Private  {
	Git::User global_user;
	Git::User repo_user;
	AvatarLoader avatar_loader;
};

SetUserDialog::SetUserDialog(BasicMainWindow *parent, Git::User const &global_user, Git::User const &repo_user, QString const &repo)
	: QDialog(parent)
	, ui(new Ui::SetUserDialog)
	, m(new Private)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	misc::setFixedSize(this);

	m->global_user = global_user;
	m->repo_user = repo_user;

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

	m->avatar_loader.start(mainwindow());
	connect(&m->avatar_loader, &AvatarLoader::updated, [&](){
		QString email = ui->lineEdit_mail->text();
		QIcon icon = m->avatar_loader.fetch(email.toStdString(), false);
		setAvatar(icon);
	});
}

SetUserDialog::~SetUserDialog()
{
	m->avatar_loader.stop();
	delete m;
	delete ui;
}

BasicMainWindow *SetUserDialog::mainwindow()
{
	return qobject_cast<BasicMainWindow *>(parent());
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

void SetUserDialog::setAvatar(QIcon const &icon)
{
	QPixmap pm = icon.pixmap(QSize(64, 64));
	ui->label_avatar->setPixmap(pm);
}

void SetUserDialog::on_radioButton_global_toggled(bool checked)
{
	if (checked) {
		ui->lineEdit_name->setText(m->global_user.name);
		ui->lineEdit_mail->setText(m->global_user.email);
	}
}

void SetUserDialog::on_radioButton_repository_toggled(bool checked)
{
	if (checked) {
		ui->lineEdit_name->setText(m->repo_user.name);
		ui->lineEdit_mail->setText(m->repo_user.email);
	}
}

void SetUserDialog::on_lineEdit_name_textChanged(QString const &text)
{
	if (isGlobalChecked()) {
		m->global_user.name = text;
	}
	if (isRepositoryChecked()) {
		m->repo_user.name = text;
	}
}

void SetUserDialog::on_lineEdit_mail_textChanged(QString const &text)
{
	if (isGlobalChecked()) {
		m->global_user.email = text;
	}
	if (isRepositoryChecked()) {
		m->repo_user.email = text;
	}
}

void SetUserDialog::on_pushButton_get_icon_clicked()
{
	ui->label_avatar->setPixmap(QPixmap());
	QString email = ui->lineEdit_mail->text();
	if (email.indexOf('@') > 0) {
		QIcon icon = m->avatar_loader.fetch(email.toStdString(), true);
		if (!icon.isNull()) {
			setAvatar(icon);
		}
	}
}

