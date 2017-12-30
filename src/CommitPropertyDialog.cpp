#include "CommitPropertyDialog.h"
#include "MainWindow.h"
#include "ui_CommitPropertyDialog.h"

#include "common/misc.h"

CommitPropertyDialog::CommitPropertyDialog(QWidget *parent, MainWindow *mw, Git::CommitItem const *commit)
	: QDialog(parent)
	, ui(new Ui::CommitPropertyDialog)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	mainwin_ = mw;
	commit_ = commit;

	ui->lineEdit_description->setText(commit->message);
	ui->lineEdit_commit_id->setText(commit->commit_id);
	ui->lineEdit_date->setText(misc::makeDateTimeString(commit->commit_date));
	ui->lineEdit_author->setText(commit->author);
	ui->lineEdit_mail->setText(commit->email);

	QString text;
	for (QString const &id : commit->parent_ids) {
		text += id + '\n';
	}
	ui->plainTextEdit_parent_ids->setPlainText(text);
}

CommitPropertyDialog::~CommitPropertyDialog()
{
	delete ui;
}

void CommitPropertyDialog::on_pushButton_checkout_clicked()
{
	mainwin_->checkout(this, commit_);
	done(QDialog::Rejected);
}
