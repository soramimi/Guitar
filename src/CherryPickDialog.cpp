#include "CherryPickDialog.h"
#include "ui_CherryPickDialog.h"

#include <common/misc.h>

CherryPickDialog::CherryPickDialog(QWidget *parent, GitCommitItem const &head, GitCommitItem const &pick, QList<GitCommitItem> parents)
	: QDialog(parent)
	, ui(new Ui::CherryPickDialog)
{
	ui->setupUi(this);

	ui->lineEdit_head_id->setText(head.commit_id.toQString(7));
	ui->lineEdit_head_message->setText(head.message);

	ui->lineEdit_pick_id->setText(pick.commit_id.toQString(7));
	ui->lineEdit_pick_message->setText(pick.message);

	QStringList cols = {
		tr("Commit"),
		tr("Date"),
		tr("Author"),
		tr("Message"),
	};

	ui->tableWidget_mainline->setColumnCount(cols.size());
	int rows = parents.size();
	ui->tableWidget_mainline->setRowCount(rows);
	auto NewQTableWidgetItem = [](QString const &text){
		QTableWidgetItem *item = new QTableWidgetItem;
		item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
		item->setText(text);
		return item;
	};
	for (int col = 0; col < cols.size(); col++) {
		ui->tableWidget_mainline->setHorizontalHeaderItem(col, NewQTableWidgetItem(cols[col]));
	}
	for (int row = 0; row < rows; row++) {
		QTableWidgetItem *item;
		int col = 0;
		auto SetItem = [&](QTableWidgetItem *item){
			ui->tableWidget_mainline->setItem(row, col++, item);
		};
		item = NewQTableWidgetItem(parents[row].commit_id.toQString(7));
		item->setData(Qt::UserRole, row + 1);
		SetItem(item);
		SetItem(NewQTableWidgetItem(misc::makeDateTimeString(parents[row].commit_date)));
		SetItem(NewQTableWidgetItem(parents[row].author));
		SetItem(NewQTableWidgetItem(parents[row].message));
	}
	ui->tableWidget_mainline->resizeColumnsToContents();
	ui->tableWidget_mainline->horizontalHeader()->setStretchLastSection(true);

	ui->tableWidget_mainline->setCurrentCell(0, 0);
}

CherryPickDialog::~CherryPickDialog()
{
	delete ui;
}

int CherryPickDialog::number() const
{
	return ui->tableWidget_mainline->currentRow() + 1;
}

bool CherryPickDialog::allowEmpty() const
{
	return ui->checkBox_allow_empty->isChecked();
}

void CherryPickDialog::on_tableWidget_mainline_itemDoubleClicked(QTableWidgetItem *)
{
	done(QDialog::Accepted);
}
