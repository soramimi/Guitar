#include "RemoteAdvancedOptionWidget.h"
#include "ui_RemoteAdvancedOptionWidget.h"

#include <QFileDialog>
#include <QStandardPaths>

RemoteAdvancedOptionWidget::RemoteAdvancedOptionWidget(QWidget *parent)
	: QWidget(parent)
	, ui(new Ui::RemoteAdvancedOptionWidget)
{
	ui->setupUi(this);
}

RemoteAdvancedOptionWidget::~RemoteAdvancedOptionWidget()
{
	delete ui;
}

void RemoteAdvancedOptionWidget::setSshKeyOverrigingEnabled(bool enabled)
{
	ui->pushButton_browse_ssh_key->setEnabled(enabled);
	ui->pushButton_clear_ssh_key->setEnabled(enabled);
	ui->lineEdit_ssh_key->setEnabled(enabled);
	ui->lineEdit_ssh_key->setText(enabled ? QString() : tr("SSH command is not registered."));
}

void RemoteAdvancedOptionWidget::on_pushButton_browse_ssh_key_clicked()
{
	QString path = QStandardPaths::locate(QStandardPaths::HomeLocation, ".ssh", QStandardPaths::LocateDirectory);
	path = QFileDialog::getOpenFileName(this, tr("SSH key override"), path);
	if (!path.isEmpty()) {
		ui->lineEdit_ssh_key->setText(path);
	}
}

void RemoteAdvancedOptionWidget::on_pushButton_clear_ssh_key_clicked()
{
	ui->lineEdit_ssh_key->clear();
}

QString RemoteAdvancedOptionWidget::sshKey() const
{
	return ui->lineEdit_ssh_key->text();
}

void RemoteAdvancedOptionWidget::setSshKey(const QString &s)
{
	ui->lineEdit_ssh_key->setText(s);
}

void RemoteAdvancedOptionWidget::clearSshKey()
{
	ui->lineEdit_ssh_key->clear();
}
