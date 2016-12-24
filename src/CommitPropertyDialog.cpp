#include "CommitPropertyDialog.h"
#include "ui_CommitPropertyDialog.h"

CommitPropertyDialog::CommitPropertyDialog(QWidget *parent, const Git::CommitItem &commit)
	: QDialog(parent)
	, ui(new Ui::CommitPropertyDialog)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	ui->lineEdit_description->setText(commit.message);
	ui->lineEdit_commit_id->setText(commit.commit_id);
	ui->lineEdit_date->setText(commit.commit_date.toLocalTime().toString(Qt::ISODate));
	ui->lineEdit_author->setText(commit.author);

	QString text;
	for (QString const &id : commit.parent_ids) {
		text += id + '\n';
	}
	ui->plainTextEdit_parent_ids->setPlainText(text);
}

CommitPropertyDialog::~CommitPropertyDialog()
{
	delete ui;
}


