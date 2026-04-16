#include "SubmodulesDialog.h"
#include "ui_SubmodulesDialog.h"
#include "MainWindow.h"
#include "common/misc.h"
#include "common/joinpath.h""

#include <QDir>

SubmodulesDialog::SubmodulesDialog(QWidget *parent, QString workingdir, std::vector<Submodule> const &mods)
	: QDialog(parent)
	, ui(new Ui::SubmodulesDialog)
	, working_dir_(workingdir)
	, mods_(mods)
{
	ui->setupUi(this);

	std::vector<QString> hedaer = {
		"Name",
		"Commit ID",
		"Date",
		"Author",
		"Message",
	};

	int rows = (int)mods_.size();
	ui->tableWidget->verticalHeader()->hide();
	ui->tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
	ui->tableWidget->setColumnCount(hedaer.size());
	ui->tableWidget->setRowCount(rows);

	for (int col = 0; col < (int)hedaer.size(); col++) {
		QTableWidgetItem *item = new QTableWidgetItem();
		item->setText(hedaer[col]);
		ui->tableWidget->setHorizontalHeaderItem(col, item);
	}

	for (int row = 0; row < rows; row++) {
		std::vector<QString> values = {
			QString::fromStdString(mods_[row].submodule.path),
			MainWindow::abbrevCommitID(mods_[row].head),
			misc::makeDateTimeString(mods_[row].head.commit_date),
			mods_[row].head.author,
			mods_[row].head.message,
		};
		for (int col = 0; col < (int)values.size(); col++) {
			QTableWidgetItem *item = new QTableWidgetItem();
			auto flags = item->flags();
			flags &= ~Qt::ItemIsEditable;
			item->setFlags(flags);
			item->setText(values[col]);
			ui->tableWidget->setItem(row, col, item);
		}
	}

	ui->tableWidget->resizeColumnsToContents();
	ui->tableWidget->horizontalHeader()->setStretchLastSection(true);

	ui->tableWidget->selectRow(0);
}

SubmodulesDialog::~SubmodulesDialog()
{
	delete ui;
}

void SubmodulesDialog::on_tableWidget_itemSelectionChanged()
{
	int row = ui->tableWidget->currentRow();
	if (row >= 0 && row < mods_.size()) {
		ui->lineEdit_path->setText(QString::fromStdString(mods_[row].submodule.path) / QString());
		ui->lineEdit_remote_url->setText(QString::fromStdString(mods_[row].submodule.url));
	}
}

QString SubmodulesDialog::absoluteDir(int row) const
{
	if (row >= 0 && row < mods_.size()) {
		QDir dir(working_dir_);
		return dir.absoluteFilePath(QString::fromStdString(mods_[row].submodule.path));
	}
	return {};
}

void SubmodulesDialog::on_pushButton_open_terminal_clicked()
{
	QString dir = absoluteDir(ui->tableWidget->currentRow());
	if (dir.isEmpty()) return;
	MainWindow::openTerminal(dir, {});
}


void SubmodulesDialog::on_pushButton_open_file_manager_clicked()
{
	QString dir = absoluteDir(ui->tableWidget->currentRow());
	if (dir.isEmpty()) return;
	MainWindow::openExplorer(dir, {});
}


void SubmodulesDialog::on_tableWidget_itemDoubleClicked(QTableWidgetItem *item)
{
	int row = ui->tableWidget->currentRow();
	if (row >= 0 && row < mods_.size()) {
		QDir d(working_dir_);
		QString dir = d.absoluteFilePath(QString::fromStdString(mods_[row].submodule.path));
		if (dir.isEmpty()) return;
		MainWindow::openNewGuitar(dir, QString::fromStdString(mods_[row].submodule.id.toString()));
	}
}

