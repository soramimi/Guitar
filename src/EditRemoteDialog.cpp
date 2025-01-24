#include "EditRemoteDialog.h"
#include "ui_EditRemoteDialog.h"
#include "MainWindow.h"
#include <QFileDialog>
#include <QStandardPaths>
#include "ApplicationGlobal.h"

EditRemoteDialog::EditRemoteDialog(MainWindow *parent, Operation op)
	: QDialog(parent)
	, ui(new Ui::EditRemoteDialog)
{
	ui->setupUi(this);

	if (op == RemoteSet) {
		ui->lineEdit_name->setReadOnly(true);
		ui->lineEdit_name->setEnabled(false);
	}

	ui->advanced_option->setSshKeyOverrigingEnabled(!global->appsettings.ssh_command.isEmpty());
}

EditRemoteDialog::~EditRemoteDialog()
{
	delete ui;
}

MainWindow *EditRemoteDialog::mainwindow()
{
	return qobject_cast<MainWindow *>(parent());
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
	ui->advanced_option->setSshKey(s);
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
	return ui->advanced_option->sshKey();
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





