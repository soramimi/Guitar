#include "CommitPropertyDialog.h"
#include "ui_CommitPropertyDialog.h"
#include "ApplicationGlobal.h"
#include "AvatarLoader.h"
#include "MainWindow.h"
#include "common/misc.h"
#include "gpg.h"
#include "main.h"

#include "UserEvent.h"

struct CommitPropertyDialog::Private {
	MainWindow *mainwindow;
	Git::CommitItem commit;
};

CommitPropertyDialog::CommitPropertyDialog(QWidget *parent, MainWindow *mw, Git::CommitItem const *commit)
	: QDialog(parent)
	, ui(new Ui::CommitPropertyDialog)
	, m(new Private)
{
	ui->setupUi(this);
	m->commit = *commit;
	init(mw);
}

CommitPropertyDialog::CommitPropertyDialog(QWidget *parent, MainWindow *mw, QString const &commit_id)
	: QDialog(parent)
	, ui(new Ui::CommitPropertyDialog)
	, m(new Private)
{
	ui->setupUi(this);

	auto commit = mw->queryCommit(commit_id);
	if (commit) {
		m->commit = *commit;
	} else {
		m->commit.commit_id = commit_id;
	}
	init(mw);
}

CommitPropertyDialog::~CommitPropertyDialog()
{
	global->avatar_loader.disconnectAvatarReady(this, &CommitPropertyDialog::avatarReady);
	delete m;
	delete ui;
}

void CommitPropertyDialog::init(MainWindow *mw)
{
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	ui->pushButton_jump->setVisible(false);

	m->mainwindow = mw;

	ui->lineEdit_message->setText(m->commit.message);
	ui->lineEdit_commit_id->setText(m->commit.commit_id.toQString());
	ui->lineEdit_date->setText(misc::makeDateTimeString(m->commit.commit_date));
	ui->lineEdit_author->setText(m->commit.author);
	ui->lineEdit_mail->setText(m->commit.email);

	// ui->label_signature_icon->setVisible(false);

	QString text;
	for (Git::CommitID const &id : m->commit.parent_ids) {
		text += id.toQString() + '\n';
	}
	ui->plainTextEdit_parent_ids->setPlainText(text);

	gpg::Data gpg;
	Git::Signature sig;
	bool gpgsig = false;
	auto s = mainwindow()->git()->log_show_signature(m->commit.commit_id);
	if (s) {
		sig = *s;
		QList<gpg::Data> keys;
		if (gpg::listKeys(global->appsettings.gpg_command, &keys)) {
			int n = sig.fingerprint.size();
			if (n > 0) {
				for (gpg::Data const &key : keys) {
					if (n == key.fingerprint.size()) {
						auto const *p1 = sig.fingerprint.data();
						auto const *p2 = key.fingerprint.data();
						if (memcmp(p1, p2, n) == 0) {
							gpg = key;
							break;
						}
					}
				}
			} else {
				qDebug() << "Failed to get gpg keys";
			}
			if (gpg.id.isEmpty()) {
				// gpgコマンドが登録されていないなど、keyidが取得できなかったとき
				gpg.id = tr("<Unknown>");
			}
		}
		gpgsig = true;
	}

	if (gpgsig) {
		ui->frame_sign->setVisible(true);
		ui->textEdit_signature->setPlainText(sig.text);
		// {
			// int w = ui->label_signature_icon->width();
			// int h = ui->label_signature_icon->width();
			// QIcon icon = mainwindow()->verifiedIcon(m->commit.signature);
			// ui->label_signature_icon->setPixmap(icon.pixmap(w, h));
		// }
		ui->lineEdit_sign_id->setText(gpg.id);
		ui->lineEdit_sign_name->setText(gpg.name);
		ui->lineEdit_sign_mail->setText(gpg.mail);
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
	qDebug() << Q_FUNC_INFO;
	updateAvatar(false);
}

MainWindow *CommitPropertyDialog::mainwindow()
{
	return m->mainwindow;
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
	mainwindow()->checkout(mainwindow()->frame(), this, &m->commit, [&](){ hide(); });
	done(QDialog::Rejected);
}

void CommitPropertyDialog::on_pushButton_jump_clicked()
{
	mainwindow()->jumpToCommit(mainwindow()->frame(), m->commit.commit_id.toQString());
	done(QDialog::Accepted);
}

void CommitPropertyDialog::on_pushButton_details_clicked()
{
	mainwindow()->execCommitViewWindow(&m->commit);
}

void CommitPropertyDialog::on_pushButton_explorer_clicked()
{
	mainwindow()->execCommitExploreWindow(mainwindow()->frame(), this, &m->commit);

}
