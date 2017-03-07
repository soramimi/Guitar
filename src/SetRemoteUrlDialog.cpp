#include "SetRemoteUrlDialog.h"
#include "ui_SetRemoteUrlDialog.h"
#include "MyTableWidgetDelegate.h"
#include "MainWindow.h"
#include "misc.h"
#include <QMessageBox>

SetRemoteUrlDialog::SetRemoteUrlDialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::SetRemoteUrlDialog)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	ui->tableWidget->setItemDelegate(new MyTableWidgetDelegate(this));

	ui->tableWidget->verticalHeader()->setVisible(false);
}

SetRemoteUrlDialog::~SetRemoteUrlDialog()
{
	delete ui;
}

GitPtr SetRemoteUrlDialog::git()
{
	return g_;
}

void SetRemoteUrlDialog::exec(MainWindow *mainwindow, GitPtr g)
{
	this->mainwindow = mainwindow;
	this->g_ = g;

	if (g->isValidWorkingCopy()) {
		QStringList remotes = g->getRemotes();
		if (remotes.size() > 0) {
			ui->lineEdit_remote->setText(remotes[0]);
		}
	}

	updateRemotesTable();
	QDialog::exec();
}

void SetRemoteUrlDialog::updateRemotesTable()
{
	QList<Git::Remote> vec;
	GitPtr g = git();
	if (g->isValidWorkingCopy()) {
		g->getRemoteURLs(&vec);
	}
	QString url;
	QString alturl;
	int rows = vec.size();
	ui->tableWidget->setColumnCount(3);
	ui->tableWidget->setRowCount(rows);
	auto newQTableWidgetItem = [](QString const &text){
		QTableWidgetItem *item = new QTableWidgetItem();
		item->setText(text);
		return item;
	};
	auto SetHeaderItem = [&](int col, QString const &text){
		ui->tableWidget->setHorizontalHeaderItem(col, newQTableWidgetItem(text));
	};
	SetHeaderItem(0, tr("Porpose"));
	SetHeaderItem(1, tr("Origin"));
	SetHeaderItem(2, tr("URL"));
	for (int row = 0; row < rows; row++) {
		Git::Remote const &r = vec[row];
		if (r.purpose == "push") {
			url = r.url;
		} else {
			alturl = r.url;
		}
		auto SetItem = [&](int col, QString const &text){
			ui->tableWidget->setItem(row, col, newQTableWidgetItem(text));
			ui->tableWidget->setRowHeight(col, 24);
		};
		SetItem(0, r.purpose);
		SetItem(1, r.remote);
		SetItem(2, r.url);
	}
	ui->tableWidget->horizontalHeader()->setStretchLastSection(true);

	if (url.isEmpty()) {
		url = alturl;
	}
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
	mainwindow->testRemoteRepositoryValidity(url);
}

