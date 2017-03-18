#include "DeleteBranchDialog.h"
#include "ui_DeleteBranchDialog.h"

struct DeleteBranchDialog::Private {
	QStringList all_local_branch_names;
	QStringList current_local_branch_names;
};

DeleteBranchDialog::DeleteBranchDialog(QWidget *parent, const QStringList &all_local_branch_names, QStringList const &current_local_branch_names) :
	QDialog(parent),
	ui(new Ui::DeleteBranchDialog)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	pv = new Private();
	pv->all_local_branch_names = all_local_branch_names;
	pv->current_local_branch_names = current_local_branch_names;

	updateList();
}

DeleteBranchDialog::~DeleteBranchDialog()
{
	delete pv;
	delete ui;
}

void DeleteBranchDialog::updateList()
{
	bool all = ui->checkBox_all_branches->isChecked();

	ui->listWidget->clear();

	for (QString const &name : all ? pv->all_local_branch_names : pv->current_local_branch_names) {
		QListWidgetItem *item = new QListWidgetItem();
		item->setText(name);
		item->setCheckState(Qt::Unchecked);
		ui->listWidget->addItem(item);
	}
}

QStringList DeleteBranchDialog::selectedBranchNames() const
{
	QStringList names;
	int n = ui->listWidget->count();
	for (int row = 0; row < n; row++) {
		QListWidgetItem *item = ui->listWidget->item(row);
		Q_ASSERT(item);
		if (item->checkState() == Qt::Checked) {
			names.push_back(item->text());
		}
	}
	return names;
}

void DeleteBranchDialog::on_checkBox_all_branches_toggled(bool /*checked*/)
{
	updateList();
}
