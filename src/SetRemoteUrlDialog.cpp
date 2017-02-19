#include "SetRemoteUrlDialog.h"
#include "ui_SetRemoteUrlDialog.h"

SetRemoteUrlDialog::SetRemoteUrlDialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::SetRemoteUrlDialog)
{
	ui->setupUi(this);

	ui->tableWidget->verticalHeader()->setVisible(false);
}

SetRemoteUrlDialog::~SetRemoteUrlDialog()
{
	delete ui;
}

void SetRemoteUrlDialog::exec(GitPtr g)
{
	this->g = g;

	QStringList remotes = g->getRemotes();
	if (remotes.size() > 0) {
		ui->lineEdit_remote->setText(remotes[0]);
	}

	updateRemotesTable();
	QDialog::exec();
}

void SetRemoteUrlDialog::updateRemotesTable()
{
	QList<Git::Remote> vec;
	g->getRemoteURLs(&vec);
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
		auto SetItem = [&](int col, QString const &text){
			ui->tableWidget->setItem(row, col, newQTableWidgetItem(text));
			ui->tableWidget->setRowHeight(col, 24);
		};
		SetItem(0, r.purpose);
		SetItem(1, r.remote);
		SetItem(2, r.url);
	}
	ui->tableWidget->horizontalHeader()->setStretchLastSection(true);
}

void SetRemoteUrlDialog::accept()
{
	QString rem = ui->lineEdit_remote->text();
	QString url = ui->lineEdit_url->text();
	g->setRemoteURL(rem, url);
	updateRemotesTable();
}


