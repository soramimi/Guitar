#include "PushDialog.h"
#include "ui_PushDialog.h"

PushDialog::PushDialog(QWidget *parent, const QStringList &remotes, const QStringList &branches) :
	QDialog(parent),
	ui(new Ui::PushDialog)
{
	ui->setupUi(this);

	if (remotes.isEmpty() || branches.isEmpty()) {
//		ui->radioButton_push_set_upstream->setEnabled(false);
	} else {
		for (QString const &remote : remotes) {
			ui->comboBox_remote->addItem(remote);
		}
		for (QString const &branch : branches) {
			ui->comboBox_branch->addItem(branch);
		}
	}

//	ui->radioButton_push_simply->click();
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

QString PushDialog::remote() const
{
	return ui->comboBox_remote->currentText();
}

QString PushDialog::branch() const
{
	return ui->comboBox_branch->currentText();
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
