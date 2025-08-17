#include "ConfigUserDialog.h"
#include "ui_ConfigUserDialog.h"
#include "ApplicationGlobal.h"
#include "AvatarLoader.h"
#include "EditProfilesDialog.h"
#include "MainWindow.h"
#include "UserEvent.h"
#include "common/misc.h"

struct ConfigUserDialog::Private  {
	GitUser global_user;
	GitUser local_user;
};

ConfigUserDialog::ConfigUserDialog(MainWindow *parent, GitUser const &global_user, GitUser const &local_user, bool enable_local_user, QString const &repo)
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

	global->avatar_loader.connectAvatarReady(this, &ConfigUserDialog::avatarReady);

	updateAvatar();
}

ConfigUserDialog::~ConfigUserDialog()
{
	global->avatar_loader.disconnectAvatarReady(this, &ConfigUserDialog::avatarReady);
	delete m;
	delete ui;
}

bool ConfigUserDialog::isLocalUnset() const
{
	return ui->checkBox_unset_local->isChecked();
}

MainWindow *ConfigUserDialog::mainwindow()
{
	return qobject_cast<MainWindow *>(parent());
}

GitUser ConfigUserDialog::user(bool global) const
{
	if (!global && ui->checkBox_unset_local->isChecked()) return {};

	GitUser user;
	if (global) { // グローバル
		user.name = ui->lineEdit_global_name->text();
		user.email = ui->lineEdit_global_email->text();
	} else { // ローカル
		if (!ui->checkBox_unset_local->isChecked()) { // 未設定でないなら
			user.name = ui->lineEdit_local_name->text();
			user.email = ui->lineEdit_local_email->text();
		}
	}
	return user;
}

void ConfigUserDialog::updateAvatar(QString const &email, bool request)
{
	QImage image = global->avatar_loader.fetch(email, request);
	ui->widget_avatar->setImage(image);
}

void ConfigUserDialog::avatarReady()
{
	updateAvatar(email(), false);
}

void ConfigUserDialog::updateAvatar()
{
	updateAvatar(email(), true);
}

QString ConfigUserDialog::email() const
{
	QString email;

	if (!ui->checkBox_unset_local->isChecked()) {
		email = ui->lineEdit_local_email->text();
		if (misc::isValidMailAddress(email)) return email;
	}

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
	(void)arg1;
	bool f = ui->checkBox_unset_local->isChecked();
	ui->lineEdit_local_name->setEnabled(!f);
	ui->lineEdit_local_email->setEnabled(!f);
	updateAvatar();
}

void ConfigUserDialog::on_pushButton_profiles_clicked()
{
	QString xmlpath = global->profiles_xml_path;
	EditProfilesDialog::Item item1;
	item1.name = ui->lineEdit_local_name->text();
	item1.email = ui->lineEdit_local_email->text();
	EditProfilesDialog dlg(this);
	dlg.loadXML(xmlpath);
	if (dlg.exec(item1) == QDialog::Accepted) {
		dlg.saveXML(xmlpath);
		EditProfilesDialog::Item item2 = dlg.selectedItem();
		ui->lineEdit_local_name->setText(item2.name);
		ui->lineEdit_local_email->setText(item2.email);
		ui->checkBox_unset_local->setChecked(false);
		updateAvatar();
	}
}

