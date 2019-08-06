#include "CommitDialog.h"
#include "ui_CommitDialog.h"
#include "BasicMainWindow.h"
#include "ConfigSigningDialog.h"
#include <QDir>

CommitDialog::CommitDialog(BasicMainWindow *parent, QString const &reponame, Git::User const &user, gpg::Data const &key)
	: QDialog(parent)
	, ui(new Ui::CommitDialog)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	key_ = key;

	ui->label_reponame->setText(reponame);
	ui->label_commit_author->setText(user.name);
	ui->label_commit_mail->setText(user.email);

	updateSigningInfo();

	ui->plainTextEdit->setFocus();
}

CommitDialog::~CommitDialog()
{
	delete ui;
}

BasicMainWindow *CommitDialog::mainwindow()
{
	return qobject_cast<BasicMainWindow *>(parent());
}

void CommitDialog::updateSigningInfo()
{
	GitPtr g = mainwindow()->git();

	Git::SignPolicy pol = g->signPolicy(Git::Source::Default);
	if (!key_.id.isEmpty()) {
		if (pol == Git::SignPolicy::True) {
			ui->groupBox_gpg_sign->setCheckable(false);
		} else {
			ui->groupBox_gpg_sign->setCheckable(true);
			ui->groupBox_gpg_sign->setChecked(false);
		}
		ui->label_sign_id->setText(key_.id);
		ui->label_sign_name->setText(key_.name);
		ui->label_sign_mail->setText(key_.mail);
	} else {
		ui->groupBox_gpg_sign->setChecked(false);
		ui->groupBox_gpg_sign->setEnabled(false);
	}
}

bool CommitDialog::isSigningEnabled() const
{
	return ui->groupBox_gpg_sign->isChecked();
}

void CommitDialog::setText(QString const &text)
{
	ui->plainTextEdit->setPlainText(text);
	QTextCursor cur = ui->plainTextEdit->textCursor();
	cur.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
	ui->plainTextEdit->setTextCursor(cur);
}

QString CommitDialog::text() const
{
	return ui->plainTextEdit->toPlainText();
}


void CommitDialog::keyPressEvent(QKeyEvent *event)
{
	if (event->modifiers() & Qt::ControlModifier) {
		if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
			event->accept();
			accept();
			return;
		}
	}
	QDialog::keyPressEvent(event);
}

void CommitDialog::on_pushButton_config_signing_clicked()
{
	ConfigSigningDialog dlg(this, mainwindow(), true);
	if (dlg.exec() == QDialog::Accepted) {
		updateSigningInfo();
	}
}
