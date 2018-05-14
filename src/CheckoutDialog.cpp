#include "CheckoutDialog.h"
#include "ui_CheckoutDialog.h"

#include <QLineEdit>

struct CheckoutDialog::Private {
	QStringList tags;
	QStringList local_branch_names;
	QStringList remote_branch_names;

};


CheckoutDialog::CheckoutDialog(QWidget *parent, const QStringList &tags, const QStringList &local_branches, const QStringList &remote_branches)
	: QDialog(parent)
	, ui(new Ui::CheckoutDialog)
	, m(new Private)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	m->tags = tags;
	m->local_branch_names = local_branches;
	m->remote_branch_names = remote_branches;

	ui->radioButton_head_detached->setText("HEAD detached");
	ui->radioButton_existing_local_branch->setText("Existing local branch");
	ui->radioButton_create_local_branch->setText("Create local branch");

	bool existing = makeComboBoxOptions(local_branches) > 0;

	if (existing) {
		ui->radioButton_existing_local_branch->click();
	} else {
		ui->radioButton_existing_local_branch->setEnabled(false);
		ui->radioButton_create_local_branch->click();
	}
}

CheckoutDialog::~CheckoutDialog()
{
	delete m;
	delete ui;
}

int CheckoutDialog::makeComboBoxOptions(QStringList const &names)
{
	int count = 0;
	ui->comboBox_branch_name->clear();
	ui->comboBox_branch_name->clearEditText();
	for (QString const &name : names) {
		int i = name.lastIndexOf('/');
		if (i < 0 && name == "HEAD") continue;
		if (i > 0 && name.mid(i + 1) == "HEAD") continue;
		ui->comboBox_branch_name->addItem(name);
		count++;
	}
	return count;
}

void CheckoutDialog::clearComboBoxOptions()
{
	makeComboBoxOptions(QStringList());
}

CheckoutDialog::Operation CheckoutDialog::operation() const
{
	if (ui->radioButton_existing_local_branch->isChecked()) return Operation::ExistingLocalBranch;
	if (ui->radioButton_create_local_branch->isChecked()) return Operation::CreateLocalBranch;
	return Operation::HeadDetached;
}

QString CheckoutDialog::branchName() const
{
	if (operation() == Operation::HeadDetached) {
		return QString();
	}
	return ui->comboBox_branch_name->currentText();
}

void CheckoutDialog::updateUI()
{
	if (ui->radioButton_head_detached->isChecked()) {
		clearComboBoxOptions();
		ui->comboBox_branch_name->setEditable(false);
		ui->comboBox_branch_name->setEnabled(false);
	} else if (ui->radioButton_existing_local_branch->isChecked()) {
		ui->comboBox_branch_name->setEnabled(true);
		ui->comboBox_branch_name->setEditable(false);
		makeComboBoxOptions(m->local_branch_names);
	} else if (ui->radioButton_create_local_branch->isChecked()) {
		ui->comboBox_branch_name->setEnabled(true);
		ui->comboBox_branch_name->setEditable(true);
		QStringList names;
		names.append(m->tags);
		names.append(m->remote_branch_names);
		makeComboBoxOptions(names);
		QLineEdit *e = ui->comboBox_branch_name->lineEdit();
		e->setFocus();
		e->selectAll();
	}
}

void CheckoutDialog::on_radioButton_head_detached_toggled(bool /*checked*/)
{
	updateUI();
}

void CheckoutDialog::on_radioButton_existing_local_branch_toggled(bool /*checked*/)
{
	updateUI();
}

void CheckoutDialog::on_radioButton_create_local_branch_toggled(bool /*checked*/)
{
	updateUI();
}
