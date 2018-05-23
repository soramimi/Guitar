#include "SelectGpgKeyDialog.h"
#include "ui_SelectGpgKeyDialog.h"
#include "MainWindow.h"

SelectGpgKeyDialog::SelectGpgKeyDialog(QWidget *parent, const QList<gpg::Data> &keys) :
	QDialog(parent),
	ui(new Ui::SelectGpgKeyDialog)
{
	ui->setupUi(this);
	keys_ = keys;
	updateTable();
}

SelectGpgKeyDialog::~SelectGpgKeyDialog()
{
	delete ui;
}

gpg::Data SelectGpgKeyDialog::key() const
{
	int row = ui->tableWidget->currentRow();
	if (row >= 0 && row < keys_.size()) {
		return keys_[row];
	}
	return gpg::Data();
}

void SelectGpgKeyDialog::updateTable()
{
	QStringList cols = {
		tr("ID"),
		tr("Name"),
		tr("Mail")
	};
	ui->tableWidget->setColumnCount(cols.size());
	ui->tableWidget->setRowCount(keys_.size());
	for (int col = 0; col < cols.size(); col++) {
		QTableWidgetItem *item = new QTableWidgetItem();
		item->setText(cols[col]);
		ui->tableWidget->setHorizontalHeaderItem(col, item);
	}
	for (int row = 0; row < keys_.size(); row++) {
		gpg::Data const &key = keys_[row];
		QTableWidgetItem *item;
		auto NewItem = [&](){
			QTableWidgetItem *item = new QTableWidgetItem();
			return item;
		};
		int col = 0;
		item = NewItem();
		item->setText(key.id);
		ui->tableWidget->setItem(row, col, item);
		col++;
		item = NewItem();
		item->setText(key.name);
		ui->tableWidget->setItem(row, col, item);
		col++;
		item = NewItem();
		item->setText(key.mail);
		ui->tableWidget->setItem(row, col, item);
//		ui->tableWidget->setRowHeight(row, 20);
	}
	ui->tableWidget->horizontalHeader()->setStretchLastSection(true);
	ui->tableWidget->resizeColumnsToContents();
}



void SelectGpgKeyDialog::on_tableWidget_itemDoubleClicked(QTableWidgetItem *item)
{
	(void)item;
	done(QDialog::Accepted);
}
