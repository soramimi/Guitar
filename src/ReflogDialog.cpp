#include "ReflogDialog.h"
#include "ui_ReflogDialog.h"

ReflogDialog::ReflogDialog(QWidget *parent, Git::ReflogItemList const &reflog)
	: QDialog(parent)
	, ui(new Ui::ReflogDialog)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	updateTable(reflog);
}

ReflogDialog::~ReflogDialog()
{
	delete ui;
}

void ReflogDialog::updateTable(Git::ReflogItemList const &reflog)
{
	QTableWidgetItem *item;
	ui->tableWidget->clear();

	QStringList cols = {
		tr("Commit"),
		tr("Head"),
		tr("Command"),
		tr("Comment"),
	};

	auto newQTableWidgetItem = [](QString const &text){
		QTableWidgetItem *item = new QTableWidgetItem(text);
		return item;
	};

	ui->tableWidget->setColumnCount(cols.size());
	ui->tableWidget->setRowCount(reflog.size());

	for (int col = 0; col < cols.size(); col++) {
		item = newQTableWidgetItem(cols[col]);
		ui->tableWidget->setHorizontalHeaderItem(col, item);
	}

	int row = 0;
	for (Git::ReflogItem const &t : reflog) {
		QString text = t.id.mid(0, 7);
		item = newQTableWidgetItem(text);
		ui->tableWidget->setItem(row, 0, item);
		item = newQTableWidgetItem(t.head);
		ui->tableWidget->setItem(row, 1, item);
		QString cmd = t.command;
		int i = cmd.indexOf(' ');
		if (i < 0) i = cmd.size();
		if (i > 10) i = 10;
		cmd = cmd.mid(0, i);
		item = newQTableWidgetItem(cmd);
		ui->tableWidget->setItem(row, 2, item);
		item = newQTableWidgetItem(t.comment);
		ui->tableWidget->setItem(row, 3, item);
		ui->tableWidget->setRowHeight(row, 24);
		row++;
	}

	ui->tableWidget->resizeColumnsToContents();
	ui->tableWidget->horizontalHeader()->setStretchLastSection(true);
}
