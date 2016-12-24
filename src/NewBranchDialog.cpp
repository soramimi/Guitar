#include "NewBranchDialog.h"
#include "ui_NewBranchDialog.h"

NewBranchDialog::NewBranchDialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::NewBranchDialog)
{
	ui->setupUi(this);
}

NewBranchDialog::~NewBranchDialog()
{
	delete ui;
}

QString NewBranchDialog::branchName() const
{
	return ui->lineEdit_branch_name->text();
}

