#include "CommitPropertyDialog.h"
#include "ui_CommitPropertyDialog.h"
#include "ApplicationGlobal.h"
#include "AvatarLoader.h"
#include "MainWindow.h"
#include "common/misc.h"
#include "gpg.h"

struct CommitPropertyDialog::Private {
	GitCommitItem commit;
};

CommitPropertyDialog::CommitPropertyDialog(QWidget *parent, GitCommitItem const &commit)
	: QDialog(parent)
	, ui(new Ui::CommitPropertyDialog)
	, m(new Private)
{
	ui->setupUi(this);
	m->commit = commit;
	init();
}

CommitPropertyDialog::~CommitPropertyDialog()
{
	global->avatar_loader.disconnectAvatarReady(this, &CommitPropertyDialog::avatarReady);
	delete m;
	delete ui;
}

void CommitPropertyDialog::init()
{
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	ui->pushButton_jump->setVisible(false);

	QString message = m->commit.message;
	QString detail;
	{
		QString s = mainwindow()->git().queryEntireCommitMessage(m->commit.commit_id);
		if (s.startsWith(message)) {
			s = s.mid(message.length());
		}
		detail = s.trimmed();
	}
	if (detail.isEmpty()) {
		ui->plainTextEdit_message_detail->setVisible(false);
	} else {
		ui->plainTextEdit_message_detail->setPlainText(detail);
	}
	
	ui->lineEdit_message->setText(message);
	ui->lineEdit_commit_id->setText(m->commit.commit_id.toQString());
	ui->lineEdit_date->setText(misc::makeDateTimeString(m->commit.commit_date));
	ui->lineEdit_author->setText(m->commit.author);
	ui->lineEdit_mail->setText(m->commit.email);

	QString text;
	for (GitHash const &id : m->commit.parent_ids) {
		text += id.toQString() + '\n';
	}
	ui->plainTextEdit_parent_ids->setPlainText(text);

	auto sig = mainwindow()->git().log_signature(m->commit.commit_id);
	if (sig) {
		QString status;
		QString sig_name;
		QString sig_mail;
		switch (sig->sign.verify) {
		case 'G':
			status = tr("Good");
			break;
		case 'B':
			status = tr("BAD");
			break;
		case 'U':
			status = tr("Unknown");
			break;
		case 'X':
			status = tr("Expired");
			break;
		case 'Y':
			status = tr("Expired");
			break;
		case 'R':
			status = tr("Revoked");
			break;
		case 'E':
			status = tr("Error");
			break;
		case 'N':
			status = tr("None");
			break;
		}
		if (status.isEmpty()) {
			status = "?";
		} else {
			status += " [" + sig->sign.trust + ']';
		}
		{
			QList<gpg::Data> keys;
			gpg::listKeys(global->appsettings.gpg_command, &keys);
			for (gpg::Data const &key : keys) {
				if (misc::compare(key.fingerprint, sig->sign.key_fingerprint) == 0) {
					sig_name = key.name;
					sig_mail = key.mail;
					break;
				}
			}
		}
		ui->lineEdit_sign_status->setText(status);
		ui->lineEdit_sign_name->setText(sig_name);
		ui->lineEdit_sign_mail->setText(sig_mail);
		ui->textEdit_sign_text->setPlainText(sig->sign.text);
		ui->frame_sign->setVisible(true);
	} else {
		ui->frame_sign->setVisible(false);
	}

	global->avatar_loader.connectAvatarReady(this, &CommitPropertyDialog::avatarReady);
	updateAvatar(true);
}

void CommitPropertyDialog::updateAvatar(bool request)
{
	if (!mainwindow()->isOnlineMode()) return;

	auto SetAvatar = [&](QString const &email, SimpleImageWidget *widget){
		if (mainwindow()->appsettings()->get_avatar_icon_from_network_enabled) {
			widget->setFixedSize(QSize(64, 64));
			QImage icon = global->avatar_loader.fetch(email, request);
			setAvatar(icon, widget);
		} else {
			widget->setVisible(false);
		}
	};
	SetAvatar(ui->lineEdit_mail->text(), ui->widget_user_avatar);
	SetAvatar(ui->lineEdit_sign_mail->text(), ui->widget_sign_avatar);
}

void CommitPropertyDialog::avatarReady()
{
	updateAvatar(false);
}

MainWindow *CommitPropertyDialog::mainwindow()
{
	return global->mainwindow;
}

void CommitPropertyDialog::setAvatar(QImage const &image, SimpleImageWidget *widget)
{
	widget->setImage(image);
}

void CommitPropertyDialog::showCheckoutButton(bool f)
{
	ui->pushButton_checkout->setVisible(f);
}

void CommitPropertyDialog::showJumpButton(bool f)
{
	ui->pushButton_jump->setVisible(f);
}

void CommitPropertyDialog::on_pushButton_checkout_clicked()
{
	mainwindow()->checkout(this, m->commit, [&](){ hide(); });
	done(QDialog::Rejected);
}

void CommitPropertyDialog::on_pushButton_jump_clicked()
{
	mainwindow()->jumpToCommit(m->commit.commit_id.toQString());
	done(QDialog::Accepted);
}

void CommitPropertyDialog::on_pushButton_details_clicked()
{
	mainwindow()->execCommitViewWindow(&m->commit);
}

void CommitPropertyDialog::on_pushButton_explorer_clicked()
{
	mainwindow()->execCommitExploreWindow(this, &m->commit);

}
