#include "CheckoutDialog.h"
#include "ui_CheckoutDialog.h"

#include <QLineEdit>

struct CheckoutDialog::Private {
	QStringList local_branch_names;
	QStringList remote_branch_names;

};


CheckoutDialog::CheckoutDialog(QWidget *parent, const QStringList &local_branch_names, const QStringList &remote_branch_names) :
	QDialog(parent),
	ui(new Ui::CheckoutDialog)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	pv = new Private();
	pv->local_branch_names = local_branch_names;
	pv->remote_branch_names = remote_branch_names;

	ui->radioButton_head_detached->setText("HEAD detached");
	ui->radioButton_existing_local_branch->setText("Existing local branch");
	ui->radioButton_create_local_branch->setText("Create local branch");

	if (makeComboBoxOptions(local_branch_names) > 0) {
		ui->radioButton_existing_local_branch->click();
	} else {
		ui->radioButton_existing_local_branch->setEnabled(false);
		ui->radioButton_create_local_branch->click();
	}
}

CheckoutDialog::~CheckoutDialog()
{
	delete pv;
	delete ui;
}

int CheckoutDialog::makeComboBoxOptions(QStringList const &names)
{
	int count = 0;
	ui->comboBox_branch_name->clear();
	ui->comboBox_branch_name->clearEditText();
	for (QString const &name : names) {
		QString text = name;
		int i = text.lastIndexOf('/');
		if (i >= 0) text = text.mid(i + 1);
		if (text == "HEAD") continue;
		ui->comboBox_branch_name->addItem(text);
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
		makeComboBoxOptions(pv->local_branch_names);
	} else if (ui->radioButton_create_local_branch->isChecked()) {
		ui->comboBox_branch_name->setEnabled(true);
		ui->comboBox_branch_name->setEditable(true);
		makeComboBoxOptions(pv->remote_branch_names);
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
