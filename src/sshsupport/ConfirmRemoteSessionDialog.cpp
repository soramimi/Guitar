#include "ConfirmRemoteSessionDialog.h"
#include "ui_ConfirmRemoteSessionDialog.h"

ConfirmRemoteSessionDialog::ConfirmRemoteSessionDialog(QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::ConfirmRemoteSessionDialog)
{
	ui->setupUi(this);
	ui->checkBox_remote_host->setChecked(false);
	ui->checkBox_remote_command->setChecked(false);
	ui->pushButton_ok->setEnabled(false);
}

ConfirmRemoteSessionDialog::~ConfirmRemoteSessionDialog()
{
	delete ui;
}

int ConfirmRemoteSessionDialog::exec(const QString &remote_host, const QString &remote_command)
{
	ui->lineEdit_remote_host->setText(remote_host);
	ui->lineEdit_remote_command->setText(remote_command);
	return QDialog::exec();
}

bool ConfirmRemoteSessionDialog::yes() const
{
	return ui->checkBox_remote_host->isChecked() && ui->checkBox_remote_command->isChecked();
}

void ConfirmRemoteSessionDialog::on_checkBox_remote_host_checkStateChanged(const Qt::CheckState &arg1)
{
	ui->pushButton_ok->setEnabled(yes());
}

void ConfirmRemoteSessionDialog::on_checkBox_remote_command_checkStateChanged(const Qt::CheckState &arg1)
{
	ui->pushButton_ok->setEnabled(yes());
}

