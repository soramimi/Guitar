#include "RebaseOntoDialog.h"
#include "ui_RebaseOntoDialog.h"

RebaseOntoDialog::RebaseOntoDialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::RebaseOntoDialog)
{
	ui->setupUi(this);
}

RebaseOntoDialog::~RebaseOntoDialog()
{
	delete ui;
}

int RebaseOntoDialog::exec(const QString &newbase, const QString &upstream, const QString &branch)
{
	ui->lineEdit_newbase->setText(newbase);
	ui->lineEdit_upstream->setText(upstream);
	ui->lineEdit_branch->setText(branch);
	return QDialog::exec();
}

QString RebaseOntoDialog::newbase() const
{
	return ui->lineEdit_newbase->text();
}

QString RebaseOntoDialog::upstream() const
{
	return ui->lineEdit_upstream->text();
}

QString RebaseOntoDialog::branch() const
{
	return ui->lineEdit_branch->text();
}
