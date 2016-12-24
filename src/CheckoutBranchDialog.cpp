#include "CheckoutBranchDialog.h"
#include "ui_CheckoutBranchDialog.h"

CheckoutBranchDialog::CheckoutBranchDialog(QWidget *parent, QList<Git::Branch> const &branches) :
	QDialog(parent),
	ui(new Ui::CheckoutBranchDialog)
{
	ui->setupUi(this);

	QString current_branch;
	for (int i = 0; i < branches.size(); i++) {
		Git::Branch const &b = branches[i];
		if (b.flags & Git::Branch::Current) {
			current_branch = b.name;
		} else if (b.name.indexOf('/') < 0) {
			ui->comboBox_branches->addItem(b.name);
		}
	}
	ui->label_current_branch->setText(current_branch);
}

CheckoutBranchDialog::~CheckoutBranchDialog()
{
	delete ui;
}

QString CheckoutBranchDialog::branchName() const
{
	return ui->comboBox_branches->currentText();
}
