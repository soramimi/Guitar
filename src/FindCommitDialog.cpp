#include "FindCommitDialog.h"
#include "ui_FindCommitDialog.h"

FindCommitDialog::FindCommitDialog(QWidget *parent, QString const &text)
	: QDialog(parent)
	, ui(new Ui::FindCommitDialog)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	ui->lineEdit->setText(text);
	ui->lineEdit->selectAll();
}

QString FindCommitDialog::text() const
{
	return ui->lineEdit->text();
}

FindCommitDialog::~FindCommitDialog()
{
	delete ui;
}
