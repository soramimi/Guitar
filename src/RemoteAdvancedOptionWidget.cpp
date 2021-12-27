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
		setSshKey(path);
	}
}

void RemoteAdvancedOptionWidget::on_pushButton_clear_ssh_key_clicked()
{
	ui->lineEdit_ssh_key->clear();
}

QString RemoteAdvancedOptionWidget::sshKey() const
{
	return ui->lineEdit_ssh_key->isEnabled() ? ui->lineEdit_ssh_key->text() : QString();
}

void RemoteAdvancedOptionWidget::updateInformation()
{
	QString path = sshKey();
	if (path.isEmpty()) {
		ui->label_information->setText({});
	} else {
		QFile file(path);
		if (file.open(QFile::ReadOnly)) {
			QString line = QString::fromLatin1(file.readLine()).trimmed();
			if (line.startsWith("-----BEGIN ") && line.endsWith("-----")) {
				QString kind = line.mid(11, line.size() - 16);
				ui->label_information->setText(tr("This file is a %1").arg(kind));
			} else {
				ui->label_information->setText(tr("This file is NOT a private key"));
			}
		} else {
			ui->label_information->setText(tr("No such file"));
		}
	}
}

void RemoteAdvancedOptionWidget::setSshKey(const QString &path)
{
	ui->lineEdit_ssh_key->setText(path);
	updateInformation();
}

void RemoteAdvancedOptionWidget::clearSshKey()
{
	ui->lineEdit_ssh_key->clear();
	updateInformation();
}

void RemoteAdvancedOptionWidget::on_lineEdit_ssh_key_textChanged(const QString &)
{
	updateInformation();
}

