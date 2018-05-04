#include "CommitDialog.h"
#include "ui_CommitDialog.h"

#include <QDir>

CommitDialog::CommitDialog(QWidget *parent, QString const &reponame, const Git::User &user, const gpg::Key &key) :
	QDialog(parent),
	ui(new Ui::CommitDialog)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	ui->label_reponame->setText(reponame);
	ui->label_commit_author->setText(user.name);
	ui->label_commit_mail->setText(user.email);

	if (key.id.isEmpty()) {
		ui->groupBox_gpg_sign->setChecked(false);
		ui->groupBox_gpg_sign->setEnabled(false);
	} else {
		ui->groupBox_gpg_sign->setChecked(true);
		ui->label_sign_id->setText(key.id);
		ui->label_sign_name->setText(key.name);
		ui->label_sign_mail->setText(key.mail);
	}

	ui->plainTextEdit->setFocus();
}

CommitDialog::~CommitDialog()
{
	delete ui;
}

bool CommitDialog::isSigningEnabled() const
{
	return ui->groupBox_gpg_sign->isChecked();
}

void CommitDialog::setText(const QString &text)
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
