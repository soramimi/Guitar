#include "FindDialog.h"
#include "ui_FindDialog.h"

FindDialog::FindDialog(QWidget *parent, QString const &text)
	: QDialog(parent)
	, ui(new Ui::FindDialog)
{
	ui->setupUi(this);
	ui->lineEdit->setText(text);
	ui->lineEdit->selectAll();
}

QString FindDialog::text() const
{
	return ui->lineEdit->text();
}

FindDialog::~FindDialog()
{
	delete ui;
}
