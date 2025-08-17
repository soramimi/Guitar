#include "CommitExploreWindow.h"
#include "ReflogWindow.h"
#include "ui_ReflogWindow.h"
#include "MainWindow.h"
#include "Git.h"
#include <QMenu>

ReflogWindow::ReflogWindow(QWidget *parent, MainWindow *mainwin, Git::ReflogItemList const &reflog)
	: QDialog(parent)
	, ui(new Ui::ReflogWindow)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	flags |= Qt::WindowMaximizeButtonHint;
	setWindowFlags(flags);

	mainwindow_ = mainwin;
	reflog_ =  reflog;

	updateTable(reflog_);
}

ReflogWindow::~ReflogWindow()
{
	delete ui;
}

void ReflogWindow::updateTable(Git::ReflogItemList const &reflog)
{
	QTableWidgetItem *item;
	ui->tableWidget->clear();

	QStringList cols = {
		tr("Commit"),
		tr("Head"),
		tr("Command"),
		tr("Message"),
	};

	auto newQTableWidgetItem = [](QString const &text){
		auto *item = new QTableWidgetItem(text);
		return item;
	};

	ui->tableWidget->setColumnCount(cols.size());
	ui->tableWidget->setRowCount(reflog.size());

	for (int col = 0; col < cols.size(); col++) {
		item = newQTableWidgetItem(cols[col]);
		ui->tableWidget->setHorizontalHeaderItem(col, item);
	}

	int row = 0;
	for (GitReflogItem const &t : reflog) {
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
		item = newQTableWidgetItem(t.message);
		ui->tableWidget->setItem(row, 3, item);
		ui->tableWidget->setRowHeight(row, 24);
		row++;
	}

	ui->tableWidget->resizeColumnsToContents();
	ui->tableWidget->horizontalHeader()->setStretchLastSection(true);
}

std::optional<GitCommitItem> ReflogWindow::currentCommitItem()
{
	int row = ui->tableWidget->currentRow();
	if (row >= 0 && row < reflog_.size()) {
		GitReflogItem const &logitem = reflog_[row];
		return mainwindow()->queryCommit(GitHash(logitem.id));
	}
	return std::nullopt;
}

void ReflogWindow::on_tableWidget_customContextMenuRequested(const QPoint &pos)
{
	std::optional<GitCommitItem> commit = currentCommitItem();
	if (!commit) return;

	QMenu menu;
	QAction *a_checkout = menu.addAction(tr("Checkout"));
	QAction *a_explorer = menu.addAction(tr("Explorer"));
	QAction *a_property = mainwindow()->addMenuActionProperty(&menu);
	QAction *a = menu.exec(ui->tableWidget->viewport()->mapToGlobal(pos) + QPoint(8, -8));
	if (a) {
		if (a == a_checkout) {
			mainwindow()->checkout(this, *commit);
			return;
		}
		if (a == a_explorer) {
			mainwindow()->execCommitExploreWindow(this, &*commit);
			return;
		}
		if (a == a_property) {
			mainwindow()->execCommitPropertyDialog(this, *commit);
			return;
		}
	}

}

void ReflogWindow::on_tableWidget_itemDoubleClicked(QTableWidgetItem *item)
{
	(void)item;
	std::optional<GitCommitItem> commit = currentCommitItem();
	if (commit) {
		mainwindow()->execCommitPropertyDialog(this, *commit);
	}
}
