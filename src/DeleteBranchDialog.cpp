#include "DeleteBranchDialog.h"
#include "ui_DeleteBranchDialog.h"

struct DeleteBranchDialog::Private {
	QStringList all_local_branch_names;
	QStringList current_local_branch_names;
	bool remote = false;
};

DeleteBranchDialog::DeleteBranchDialog(QWidget *parent, bool remote, QStringList const &all_local_branch_names, QStringList const &current_local_branch_names)
	: QDialog(parent)
	, ui(new Ui::DeleteBranchDialog)
	, m(new Private)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	m->remote = remote;

	if (isRemote()) {
		setWindowTitle(tr("Delete Remote Branch"));
//		ui->checkBox_all_branches->setVisible(false);
	}

	m->all_local_branch_names = all_local_branch_names;
	m->current_local_branch_names = current_local_branch_names;

	updateList();
}

DeleteBranchDialog::~DeleteBranchDialog()
{
	delete m;
	delete ui;
}

bool DeleteBranchDialog::isRemote() const
{
	return m->remote;
}

void DeleteBranchDialog::updateList()
{
	bool all = ui->checkBox_all_branches->isChecked();

	ui->listWidget->clear();

	for (QString const &name : (all ? m->all_local_branch_names : m->current_local_branch_names)) {
		auto *item = new QListWidgetItem;
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
