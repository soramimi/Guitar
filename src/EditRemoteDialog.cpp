#include "EditRemoteDialog.h"
#include "ui_EditRemoteDialog.h"

#include "BasicMainWindow.h"

EditRemoteDialog::EditRemoteDialog(BasicMainWindow *parent, Operation op) :
	QDialog(parent),
	ui(new Ui::EditRemoteDialog)
{
	ui->setupUi(this);

	if (op == RemoteSet) {
		ui->lineEdit_name->setReadOnly(true);
		ui->lineEdit_name->setEnabled(false);
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

QString EditRemoteDialog::name() const
{
	return ui->lineEdit_name->text();
}

QString EditRemoteDialog::url() const
{
	return ui->lineEdit_url->text();
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
	if (mainwindow()->testRemoteRepositoryValidity(url, {})) {
		ui->pushButton_ok->setFocus();
	} else {
		ui->lineEdit_url->setFocus();
	}
}
