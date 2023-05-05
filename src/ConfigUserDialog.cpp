#include "ConfigUserDialog.h"
#include "ui_ConfigUserDialog.h"
#include "AvatarLoader.h"
#include "MainWindow.h"
#include "common/misc.h"
#include "UserEvent.h"

struct ConfigUserDialog::Private  {
	Git::User global_user;
	Git::User local_user;
	AvatarLoader avatar_loader;
};

ConfigUserDialog::ConfigUserDialog(MainWindow *parent, Git::User const &global_user, Git::User const &local_user, bool enable_local_user, QString const &repo)
	: QDialog(parent)
	, ui(new Ui::ConfigUserDialog)
	, m(new Private)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	misc::setFixedSize(this);

	m->global_user = global_user;
	m->local_user = local_user;
	ui->lineEdit_global_name->setText(m->global_user.name);
	ui->lineEdit_global_email->setText(m->global_user.email);
	ui->lineEdit_local_name->setText(m->local_user.name);
	ui->lineEdit_local_email->setText(m->local_user.email);

	ui->groupBox_local->setEnabled(enable_local_user);

	ui->checkBox_unset_local->setChecked(!misc::isValidMailAddress(m->local_user.email));

	QString text = tr("Local");
	if (!repo.isEmpty()) {
		text = QString("%1 : (%2)").arg(text).arg(repo);
	}
	ui->groupBox_local->setTitle(text);

	m->avatar_loader.addListener(this);
	m->avatar_loader.start(mainwindow());
}

ConfigUserDialog::~ConfigUserDialog()
{
	m->avatar_loader.stop();
	m->avatar_loader.removeListener(this);
	delete m;
	delete ui;
}

MainWindow *ConfigUserDialog::mainwindow()
{
	return qobject_cast<MainWindow *>(parent());
}

Git::User ConfigUserDialog::user(bool global) const
{
	if (!global && ui->checkBox_unset_local->isChecked()) return {};

	Git::User user;
	user.name  = (global ? ui->lineEdit_global_name  : ui->lineEdit_local_name )->text();
	user.email = (global ? ui->lineEdit_global_email : ui->lineEdit_local_email)->text();
	return user;
}

void ConfigUserDialog::updateAvatar(QString const &email, bool request)
{
	QImage image = m->avatar_loader.fetchImage(email.toStdString(), request);
	ui->widget_avatar->setImage(image);
}

QString ConfigUserDialog::email() const
{
	QString email;

	email = ui->lineEdit_local_email->text();
	if (misc::isValidMailAddress(email)) return email;

	email = ui->lineEdit_global_email->text();
	if (misc::isValidMailAddress(email)) return email;

	return {};
}

void ConfigUserDialog::on_pushButton_get_icon_clicked()
{
	ui->widget_avatar->setImage({});
	QString email = this->email();
	if (misc::isValidMailAddress(email)) {
		updateAvatar(email, true);
	}
}

void ConfigUserDialog::customEvent(QEvent *event)
{
	if (event->type() == (QEvent::Type)UserEvent::AvatarReady) {
		updateAvatar(email(), false);
		return;
	}
}

void ConfigUserDialog::on_lineEdit_global_name_textChanged(const QString &text)
{
	m->global_user.name = text;
}


void ConfigUserDialog::on_lineEdit_global_email_textEdited(const QString &text)
{
	m->global_user.email = text;
}

void ConfigUserDialog::on_lineEdit_local_name_textEdited(const QString &text)
{
	m->local_user.name = text;
}


void ConfigUserDialog::on_lineEdit_local_email_textEdited(const QString &text)
{
	m->local_user.email = text;

	bool f = misc::isValidMailAddress(m->local_user.email);
	bool b = ui->checkBox_unset_local->blockSignals(true);
	ui->checkBox_unset_local->setChecked(!f);
	ui->checkBox_unset_local->blockSignals(b);
}


void ConfigUserDialog::on_checkBox_unset_local_stateChanged(int arg1)
{
	if (ui->checkBox_unset_local->isChecked()) {
		ui->lineEdit_local_name->clear();
		ui->lineEdit_local_email->clear();
	}
}

