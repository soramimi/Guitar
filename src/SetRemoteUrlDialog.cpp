#include "SetRemoteUrlDialog.h"
#include "ui_SetRemoteUrlDialog.h"
#include "MainWindow.h"
#include "MyTableWidgetDelegate.h"
#include "common/misc.h"
#include <QMessageBox>

SetRemoteUrlDialog::SetRemoteUrlDialog(MainWindow *mainwindow, QStringList const &remotes, GitPtr const &g)
	: BasicRepositoryDialog(mainwindow, g)
	, ui(new Ui::SetRemoteUrlDialog)
{
	ui->setupUi(this);

	this->remotes = remotes;
}

SetRemoteUrlDialog::~SetRemoteUrlDialog()
{
	delete ui;
}

int SetRemoteUrlDialog::exec()
{
	GitPtr g = git();
	if (g->isValidWorkingCopy()) {
		QString remote = remotes.isEmpty() ? "origin" : remotes[0];
		ui->lineEdit_name->setText(remote);
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
		QString rem = ui->lineEdit_name->text();
		QString url = ui->lineEdit_url->text();
		Git::Remote r;
		r.name = rem;
		r.url = url;
		if (remotes.contains(rem)) {
			g->setRemoteURL(r);
		} else {
			g->addRemoteURL(r);
		}
		updateRemotesTable();
	}
	QDialog::done(Accepted);
}

void SetRemoteUrlDialog::on_pushButton_test_clicked()
{
	QString url = ui->lineEdit_url->text();
	mainwindow()->testRemoteRepositoryValidity(url, {});
}


