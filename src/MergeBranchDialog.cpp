#include "MergeBranchDialog.h"
#include "ui_MergeBranchDialog.h"

MergeBranchDialog::MergeBranchDialog(QWidget *parent, const QList<Git::Branch> &branches) :
	QDialog(parent),
	ui(new Ui::MergeBranchDialog)
{
	ui->setupUi(this);

	QString current_branch;
	for (const auto & b : branches) {
		if (b.isCurrent()) {
			current_branch = b.name;
		} else if (b.name.indexOf('/') < 0) {
			ui->comboBox_branches->addItem(b.name);
		}
	}
	ui->label_current_branch->setText(current_branch);
}

MergeBranchDialog::~MergeBranchDialog()
{
	delete ui;
}

QString MergeBranchDialog::branchName() const
{
	return ui->comboBox_branches->currentText();
}
