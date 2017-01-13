#include "FileHistoryWindow.h"
#include "MainWindow.h"
#include "ui_FileHistoryWindow.h"
#include "misc.h"
#include "GitDiff.h"
#include "joinpath.h"
#include "FileDiffWidget.h"

FileHistoryWindow::FileHistoryWindow(QWidget *parent, GitPtr g, const QString &path)
	: QDialog(parent)
	, ui(new Ui::FileHistoryWindow)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	mainwindow = qobject_cast<MainWindow *>(parent);
	Q_ASSERT(mainwindow);

	ui->splitter->setSizes({100, 100});

	ui->widget_diff_view->bind(mainwindow);

	Q_ASSERT(g);
	Q_ASSERT(g->isValidWorkingCopy());
	this->g = g;
	this->path = path;


	collectFileHistory();

	updateDiffView();
}

FileHistoryWindow::~FileHistoryWindow()
{
	delete ui;
}

void FileHistoryWindow::collectFileHistory()
{
	commit_item_list = g->log_all(path, mainwindow->limitLogCount(), mainwindow->limitLogTime());

	QStringList cols = {
		tr("Commit"),
		tr("Date"),
		tr("Author"),
		tr("Description"),
	};
	int n = cols.size();
	ui->tableWidget_log->setColumnCount(n);
	ui->tableWidget_log->setRowCount(0);
	for (int i = 0; i < n; i++) {
		QString const &text = cols[i];
		QTableWidgetItem *item = new QTableWidgetItem(text);
		ui->tableWidget_log->setHorizontalHeaderItem(i, item);
	}

	int count = commit_item_list.size();
	ui->tableWidget_log->setRowCount(count);

	for (int row = 0; row < count; row++) {
		Git::CommitItem const &commit = commit_item_list[row];
		int col = 0;
		auto AddColumn = [&](QString const &text){
			QTableWidgetItem *item = new QTableWidgetItem(text);
			ui->tableWidget_log->setItem(row, col, item);
			col++;
		};

		QString commit_id = mainwindow->abbrevCommitID(commit);
		QString datetime = misc::makeDateTimeString(commit.commit_date);
		AddColumn(commit_id);
		AddColumn(datetime);
		AddColumn(commit.author);
		AddColumn(commit.message);
		ui->tableWidget_log->setRowHeight(row, 24);
	}

	ui->tableWidget_log->resizeColumnsToContents();
	ui->tableWidget_log->horizontalHeader()->setStretchLastSection(false);
	ui->tableWidget_log->horizontalHeader()->setStretchLastSection(true);

	ui->tableWidget_log->setFocus();
	ui->tableWidget_log->setCurrentCell(0, 0);
}

void FileHistoryWindow::updateDiffView()
{
	ui->widget_diff_view->clearDiffView();

	int row = ui->tableWidget_log->currentRow();
	if (row >= 0 && row + 1 < (int)commit_item_list.size()) {
		Git::CommitItem const &commit_left = commit_item_list[row + 1]; // older
		Git::CommitItem const &commit_right = commit_item_list[row];    // newer

		QString id_left = GitDiff().findFileID(g, commit_left.commit_id, path);
		QString id_right = GitDiff().findFileID(g, commit_right.commit_id, path);

		ui->widget_diff_view->updateDiffView(id_left, id_right);
	}
}

void FileHistoryWindow::on_tableWidget_log_currentItemChanged(QTableWidgetItem * /*current*/, QTableWidgetItem * /*previous*/)
{
	updateDiffView();
}

