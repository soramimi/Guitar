#include "CommitPropertyDialog.h"
#include "MainWindow.h"
#include "gpg.h"
#include "ui_CommitPropertyDialog.h"
#include "ApplicationGlobal.h"
#include "AvatarLoader.h"
#include "common/misc.h"

struct CommitPropertyDialog::Private {
	MainWindow *mainwindow;
	Git::CommitItem commit;
	AvatarLoader avatar_loader;
};

void CommitPropertyDialog::init(MainWindow *mw)
{
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	ui->pushButton_jump->setVisible(false);

	m->mainwindow = mw;

	ui->lineEdit_description->setText(m->commit.message);
	ui->lineEdit_commit_id->setText(m->commit.commit_id);
	ui->lineEdit_date->setText(misc::makeDateTimeString(m->commit.commit_date));
	ui->lineEdit_author->setText(m->commit.author);
	ui->lineEdit_mail->setText(m->commit.email);

	QString text;
	for (QString const &id : m->commit.parent_ids) {
		text += id + '\n';
	}
	ui->plainTextEdit_parent_ids->setPlainText(text);

	gpg::Key key;
	int n1 = m->commit.fingerprint.size();
	if (m->commit.verified && n1 > 0) {
		QList<gpg::Key> keys;
		gpg::listKeys(global->gpg_command, &keys);
		for (gpg::Key const &k : keys) {
			int n2 = k.fingerprint.size();
			if (n2 > 0) {
				int n = std::min(n1, n2);
				char const *p1 = m->commit.fingerprint.data() + n1 - n;
				char const *p2 = k.fingerprint.data() + n2 - n;
				if (memcmp(p1, p2, n) == 0) {
					key = k;
					break;
				}
			}
		}
	}
	if (key.id.isEmpty()) {
		ui->frame_sign->setVisible(false);
	} else {
		ui->lineEdit_sign_id->setText(key.id);
		ui->lineEdit_sign_name->setText(key.name);
		ui->lineEdit_sign_mail->setText(key.mail);
	}

	m->avatar_loader.start(mainwindow()->webContext());
	connect(&m->avatar_loader, &AvatarLoader::updated, [&](){
		UpdateAvatar(false);
	});
	UpdateAvatar(true);
}

void CommitPropertyDialog::UpdateAvatar(bool request)
{
	auto SetAvatar = [&](QString const &email, QLabel *label){
		if (mainwindow()->appsettings()->get_committer_icon) {
			label->setFixedSize(QSize(48, 48));
			QIcon icon = m->avatar_loader.fetch(email.toStdString(), request);
			setAvatar(icon, label);
		} else {
			label->setVisible(false);
		}
	};
	SetAvatar(ui->lineEdit_mail->text(), ui->label_user_avatar);
	SetAvatar(ui->lineEdit_sign_mail->text(), ui->label_sign_avatar);
}

CommitPropertyDialog::CommitPropertyDialog(QWidget *parent, MainWindow *mw, Git::CommitItem const *commit)
	: QDialog(parent)
	, ui(new Ui::CommitPropertyDialog)
	, m(new Private)
{
	ui->setupUi(this);
	m->commit = *commit;
	init(mw);
}

CommitPropertyDialog::CommitPropertyDialog(QWidget *parent, MainWindow *mw, const QString &commit_id)
	: QDialog(parent)
	, ui(new Ui::CommitPropertyDialog)
	, m(new Private)
{
	ui->setupUi(this);

	mw->queryCommit(commit_id, &m->commit);
	init(mw);
}

CommitPropertyDialog::~CommitPropertyDialog()
{
	m->avatar_loader.interrupt();
	m->avatar_loader.wait();
	delete m;
	delete ui;
}

MainWindow *CommitPropertyDialog::mainwindow()
{
	return m->mainwindow;
}

void CommitPropertyDialog::setAvatar(QIcon icon, QLabel *label)
{
	QPixmap pm = icon.pixmap(label->size());
	label->setPixmap(pm);
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
	mainwindow()->checkout(this, &m->commit);
	done(QDialog::Rejected);
}

void CommitPropertyDialog::on_pushButton_jump_clicked()
{
	mainwindow()->jumpToCommit(m->commit.commit_id);
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
