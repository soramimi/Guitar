#include "SubmodulesDialog.h"
#include "ui_SubmodulesDialog.h"
#include "BasicMainWindow.h"
#include "common/misc.h"

SubmodulesDialog::SubmodulesDialog(QWidget *parent, std::vector<Submodule> mods)
	: QDialog(parent)
	, ui(new Ui::SubmodulesDialog)
{
	ui->setupUi(this);

	int rows = (int)mods.size();
	ui->tableWidget->setColumnCount(5);
	ui->tableWidget->setRowCount(rows);
	for (int row = 0; row < rows; row++) {
		QTableWidgetItem *item;

		item = new QTableWidgetItem();
		item->setText(mods[row].submodule.path);
		ui->tableWidget->setItem(row, 0, item);

		item = new QTableWidgetItem();
		item->setText(BasicMainWindow::abbrevCommitID(mods[row].head));
		ui->tableWidget->setItem(row, 1, item);

		item = new QTableWidgetItem();
		item->setText(misc::makeDateTimeString(mods[row].head.commit_date));
		ui->tableWidget->setItem(row, 2, item);

		item = new QTableWidgetItem();
		item->setText(mods[row].head.author);
		ui->tableWidget->setItem(row, 3, item);

		item = new QTableWidgetItem();
		item->setText(mods[row].head.message);
		ui->tableWidget->setItem(row, 4, item);
	}

	ui->tableWidget->resizeColumnsToContents();
	ui->tableWidget->horizontalHeader()->setStretchLastSection(true);
}

SubmodulesDialog::~SubmodulesDialog()
{
	delete ui;
}
