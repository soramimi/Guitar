#include "PushDialog.h"
#include "ui_PushDialog.h"

PushDialog::PushDialog(QWidget *parent, QStringList const &remotes, QStringList const &branches, const RemoteBranch &remote_branch) :
	QDialog(parent),
	ui(new Ui::PushDialog)
{
	ui->setupUi(this);

	if (remotes.isEmpty() || branches.isEmpty()) {
		// thru
	} else {
		for (QString const &remote : remotes) {
			ui->comboBox_remote->addItem(remote);
		}
		for (QString const &branch : branches) {
			ui->comboBox_branch->addItem(branch);
		}
	}

	if (!remote_branch.remote.isEmpty()) {
		ui->comboBox_remote->setCurrentText(remote_branch.remote);
	}
	if (!remote_branch.branch.isEmpty()) {
		ui->comboBox_branch->setCurrentText(remote_branch.branch);
	}

	updateUI();
}

PushDialog::~PushDialog()
{
	delete ui;
}

PushDialog::Action PushDialog::action() const
{
#if 1
	return PushSetUpstream;
#else
	if (ui->radioButton_push_set_upstream->isChecked()) {
		return PushSetUpstream;
	}
	return PushSimple;
#endif
}

bool PushDialog::isSetUpStream() const
{
	return ui->groupBox_set_upstream->isChecked();
}

QString PushDialog::remote() const
{
	return isSetUpStream() ? ui->comboBox_remote->currentText() : QString();
}

QString PushDialog::branch() const
{
	return isSetUpStream() ? ui->comboBox_branch->currentText() : QString();
}

void PushDialog::updateUI()
{
	bool ok = true;
	if (ui->checkBox_force->isChecked()) {
		ui->checkBox_really_force->setVisible(true);
		if (!ui->checkBox_really_force->isChecked()) {
			ok = false;
		}
	} else {
		ui->checkBox_really_force->setVisible(false);
	}
	ui->pushButton_ok->setEnabled(ok);
}

bool PushDialog::isForce() const
{
	return ui->checkBox_force->isChecked() && ui->checkBox_really_force->isChecked();
}

#if 0
void PushDialog::on_radioButton_push_simply_clicked()
{
	ui->frame_set_upstream->setEnabled(false);
}

void PushDialog::on_radioButton_push_set_upstream_clicked()
{
	ui->frame_set_upstream->setEnabled(true);
}
#endif

void PushDialog::on_checkBox_force_clicked()
{
	updateUI();
}


void PushDialog::on_checkBox_really_force_clicked()
{
	updateUI();
}

