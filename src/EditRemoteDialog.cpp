#include "EditRemoteDialog.h"
#include "ui_EditRemoteDialog.h"
#include "BasicMainWindow.h"
#include <QFileDialog>
#include <QStandardPaths>

EditRemoteDialog::EditRemoteDialog(BasicMainWindow *parent, Operation op, const Git::Context *gcx)
	: QDialog(parent)
	, ui(new Ui::EditRemoteDialog)
{
	ui->setupUi(this);

	if (op == RemoteSet) {
		ui->lineEdit_name->setReadOnly(true);
		ui->lineEdit_name->setEnabled(false);
	}

	if (gcx->ssh_command.isEmpty()) {
		ui->pushButton_override_ssh_key->setEnabled(false);
		ui->pushButton_clear_ssh_key->setEnabled(false);
		ui->lineEdit_ssh_key->setEnabled(false);
		ui->lineEdit_ssh_key->setText(tr("SSH command is not registered."));
	}

}

EditRemoteDialog::~EditRemoteDialog()
{
	delete ui;
}

BasicMainWindow *EditRemoteDialog::mainwindow()
{
	return qobject_cast<BasicMainWindow *>(parent());
}

void EditRemoteDialog::setName(QString const &s) const
{
	ui->lineEdit_name->setText(s);
}

void EditRemoteDialog::setUrl(QString const &s) const
{
	ui->lineEdit_url->setText(s);
}

void EditRemoteDialog::setSshKey(QString const &s) const
{
	ui->lineEdit_ssh_key->setText(s);
}

QString EditRemoteDialog::name() const
{
	return ui->lineEdit_name->text();
}

QString EditRemoteDialog::url() const
{
	return ui->lineEdit_url->text();
}

QString EditRemoteDialog::sshKey() const
{
	return ui->lineEdit_ssh_key->text();
}

int EditRemoteDialog::exec()
{
	if (ui->lineEdit_name->text().isEmpty()) {
		ui->lineEdit_name->setFocus();
	} else {
		ui->lineEdit_url->setFocus();
	}
	return QDialog::exec();
}

void EditRemoteDialog::on_pushButton_test_clicked()
{
	QString url = ui->lineEdit_url->text();
	if (mainwindow()->testRemoteRepositoryValidity(url, sshKey())) {
		ui->pushButton_ok->setFocus();
	} else {
		ui->lineEdit_url->setFocus();
	}
}

void EditRemoteDialog::on_pushButton_clear_ssh_key_clicked()
{
	ui->lineEdit_ssh_key->clear();
}


void EditRemoteDialog::on_pushButton_override_ssh_key_clicked()
{
	QString path = QStandardPaths::locate(QStandardPaths::HomeLocation, ".ssh", QStandardPaths::LocateDirectory);
	path = QFileDialog::getOpenFileName(this, tr("SSH key override"), path);
	if (!path.isEmpty()) {
		ui->lineEdit_ssh_key->setText(path);
	}
}
