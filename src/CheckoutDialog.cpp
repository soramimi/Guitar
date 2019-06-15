#include "CheckoutDialog.h"
#include "ui_CheckoutDialog.h"
#include <set>
#include <QLineEdit>

struct CheckoutDialog::Private {
	QStringList tags;
	QStringList all_local_branch_names;
	QStringList local_branch_names;
	QStringList remote_branch_names;

};

CheckoutDialog::CheckoutDialog(QWidget *parent, QStringList const &tags, QStringList const &all_local_branches, QStringList const &local_branches, QStringList const &remote_branches)
	: QDialog(parent)
	, ui(new Ui::CheckoutDialog)
	, m(new Private)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	m->tags = tags;
	m->all_local_branch_names = all_local_branches;
	m->local_branch_names = local_branches;
	m->remote_branch_names = remote_branches;

	ui->radioButton_head_detached->setText("HEAD detached");
	ui->radioButton_existing_local_branch->setText("Existing local branch");
	ui->radioButton_create_local_branch->setText("Create local branch");

	makeComboBoxOptions(true);
}

CheckoutDialog::~CheckoutDialog()
{
	delete m;
	delete ui;
}

int CheckoutDialog::makeComboBoxOptionsFromList(QStringList const &names)
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

CheckoutDialog::Operation CheckoutDialog::makeComboBoxOptions(bool click)
{
	// ローカルブランチがあればそれを優先して選択
	if (makeComboBoxOptionsFromList(m->local_branch_names) > 0) {
		auto *rb = ui->radioButton_existing_local_branch;
		rb->setFocus();
		if (click) rb->click();
		return Operation::ExistingLocalBranch;
	}

	// 全ローカルブランチ名をコンボボックスに追加
	bool f = makeComboBoxOptionsFromList(m->all_local_branch_names);
	ui->radioButton_existing_local_branch->setEnabled(f);

	// リモートブランチ名と同じローカルブランチが既存なら
	for (QString const &name : m->all_local_branch_names) {
		if (m->remote_branch_names.indexOf(name) >= 0) {
			ui->comboBox_branch_name->setCurrentText(name);
			{
				auto *rb = ui->radioButton_existing_local_branch;
				rb->setFocus();
				if (click) rb->click();
			}
			return Operation::ExistingLocalBranch;
		}
	}

	// ブランチ新規作成
	{
		auto *rb = ui->radioButton_create_local_branch;
		rb->setFocus();
		if (click) rb->click();
	}
	return Operation::CreateLocalBranch;
}

void CheckoutDialog::clearComboBoxOptions()
{
	ui->comboBox_branch_name->clear();
	ui->comboBox_branch_name->clearEditText();
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
		makeComboBoxOptions(false);
	} else if (ui->radioButton_create_local_branch->isChecked()) {
		ui->comboBox_branch_name->setEnabled(true);
		ui->comboBox_branch_name->setEditable(true);
		QStringList names;
		names.append(m->tags);
		names.append(m->remote_branch_names);
		makeComboBoxOptionsFromList(names);
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
