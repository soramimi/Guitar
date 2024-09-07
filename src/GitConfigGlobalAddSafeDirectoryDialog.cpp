#include "GitConfigGlobalAddSafeDirectoryDialog.h"
#include "ui_GitConfigGlobalAddSafeDirectoryDialog.h"

GitConfigGlobalAddSafeDirectoryDialog::GitConfigGlobalAddSafeDirectoryDialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::GitConfigGlobalAddSafeDirectoryDialog)
{
	ui->setupUi(this);
}

GitConfigGlobalAddSafeDirectoryDialog::~GitConfigGlobalAddSafeDirectoryDialog()
{
	delete ui;
}

void GitConfigGlobalAddSafeDirectoryDialog::setMessage(const QString &message, const QString &command)
{
	ui->plainTextEdit->setPlainText(message);
	ui->lineEdit->setText(command);
}

void GitConfigGlobalAddSafeDirectoryDialog::on_checkBox_confirm_checkStateChanged(const Qt::CheckState &arg1)
{
	ui->pushButton_ok->setEnabled(ui->checkBox_confirm->isChecked());

}

