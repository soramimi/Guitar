#include "SetRemoteUrlDialog.h"
#include "ui_SetRemoteUrlDialog.h"
#include "MyTableWidgetDelegate.h"
#include "MainWindow.h"
#include "misc.h"
#include <QMessageBox>

SetRemoteUrlDialog::SetRemoteUrlDialog(MainWindow *mainwindow, GitPtr g) :
	BasicRepositoryDialog(mainwindow, g),
	ui(new Ui::SetRemoteUrlDialog)
{
	ui->setupUi(this);
}

SetRemoteUrlDialog::~SetRemoteUrlDialog()
{
	delete ui;
}

int SetRemoteUrlDialog::exec()
{
	GitPtr g = git();
	if (g->isValidWorkingCopy()) {
		QStringList remotes = g->getRemotes();
		if (remotes.size() > 0) {
			ui->lineEdit_remote->setText(remotes[0]);
		}
	}

	updateRemotesTable();
	return QDialog::exec();
}

void SetRemoteUrlDialog::updateRemotesTable()
{
	QString url = BasicRepositoryDialog::updateRemotesTable(ui->tableWidget);
	ui->lineEdit_url->setText(url);
}



void SetRemoteUrlDialog::accept()
{
	GitPtr g = git();
	if (g->isValidWorkingCopy()) {
		QString rem = ui->lineEdit_remote->text();
		QString url = ui->lineEdit_url->text();
		g->setRemoteURL(rem, url);
		updateRemotesTable();
	}
	QDialog::accept();
}

void SetRemoteUrlDialog::on_pushButton_test_clicked()
{
	QString url = ui->lineEdit_url->text();
	mainwindow()->testRemoteRepositoryValidity(url);
}


