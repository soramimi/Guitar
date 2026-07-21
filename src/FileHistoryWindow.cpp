#include "FileHistoryWindow.h"
#include "ui_FileHistoryWindow.h"
#include "FileDiffWidget.h"
#include "MainWindow.h"
#include "MyTableWidgetDelegate.h"
#include "OverrideWaitCursor.h"
#include <common/q/helper.h>
#include <QMenu>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QThread>

struct FileHistoryWindow::Private {
	GitRunner g;
	QString path;
	GitCommitItemList commit_item_list;
	FileDiffWidget::DiffData diff_data;
};

FileDiffWidget::DiffData *FileHistoryWindow::diffdata()
{
	return &m->diff_data;
}

const FileDiffWidget::DiffData *FileHistoryWindow::diffdata() const
{
	return &m->diff_data;
}

int FileHistoryWindow::totalTextLines() const
{
	return (int)diffdata()->left->lines.size();
}

FileHistoryWindow::FileHistoryWindow(MainWindow *parent)
	: QDialog(parent)
	, ui(new Ui::FileHistoryWindow)
	, m(new Private)
{
	ui->setupUi(this);
	ui->tableWidget_log->setItemDelegate(new MyTableWidgetDelegate(this));
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	flags |= Qt::WindowMaximizeButtonHint;
	setWindowFlags(flags);

	ui->splitter->setSizes({100, 200});

	ui->widget_diff_view->init();
}

FileHistoryWindow::~FileHistoryWindow()
{
	delete m;
	delete ui;
}

MainWindow *FileHistoryWindow::mainwindow()
{
	return qobject_cast<MainWindow *>(parent());
}

void FileHistoryWindow::prepare(GitRunner g, QString const &path)
{
	Q_ASSERT(g.isValidWorkingCopy());
	this->m->g = g;
	this->m->path = path;

	QString reponame = mainwindow()->currentRepositoryName();
	QString brname = (QS)mainwindow()->currentBranch().name;

	QString text = "%1 (%2)";
	text = text.arg(reponame).arg(brname);
	ui->label_repo->setText(text);
	ui->label_path->setText(path);

	{
		OverrideWaitCursor;
		m->commit_item_list = m->g.log_file(m->path.toStdString(), mainwindow()->limitLogCount());
	}

	collectFileHistory();

	updateDiffView();
}

void FileHistoryWindow::collectFileHistory()
{
	QStringList cols = {
		tr("Commit"),
		tr("Date"),
		tr("Author"),
		tr("Message"),
	};
	int n = cols.size();
	ui->tableWidget_log->setColumnCount(n);
	ui->tableWidget_log->setRowCount(0);
	
	for (int i = 0; i < n; i++) {
		QString const &text = cols[i];
		auto *item = new QTableWidgetItem(text);
		ui->tableWidget_log->setHorizontalHeaderItem(i, item);
	}
	
	std::span<GitCommitItem const *const> items = m->commit_item_list.items();
	
	ui->tableWidget_log->setRowCount((int)items.size());

	for (int row = 0; row < (int)items.size(); row++) {
		GitCommitItem const *commit = items[row];
		int col = 0;
		auto AddColumn = [&](QString const &text, QString const &tooltip){
			auto *item = new QTableWidgetItem(text);
			item->setToolTip(tooltip);
			ui->tableWidget_log->setItem(row, col, item);
			col++;
		};
		
		QString commit_id = MainWindow::abbrevCommitID(*commit);
		QString datetime = (QS)commit->commit_date.toString();
		AddColumn(commit_id, QString());
		AddColumn(datetime, QString());
		AddColumn((QS)commit->author, QString());
		AddColumn((QS)commit->message, (QS)commit->message);
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
	Q_ASSERT(m->g.isValidWorkingCopy());

	ui->widget_diff_view->clearDiffView();
	
	auto FindFileID = [&](GitCommitItem const &commit){
		return mainwindow()->findFileID(commit.commit_id, m->path).toStdString();
	};
	
	std::span<GitCommitItem const *const> items = m->commit_item_list.items();
	
	int row = ui->tableWidget_log->currentRow();
	if (row >= 0 && row + 1 < (int)items.size()) {
		GitCommitItem const *commit_left = items[row + 1]; // older
		GitCommitItem const *commit_right = items[row];    // newer

		std::string id_left = FindFileID(*commit_left);
		std::string id_right = FindFileID(*commit_right);

		ui->widget_diff_view->updateDiffView(id_left, id_right, m->path.toStdString());
	} else if (row >= 0 && row < (int)items.size()) {
		GitCommitItem const *commit = items[row];    // newer
		std::string id = FindFileID(*commit);

		GitDiff diff(id, m->path.toStdString(), {});
		ui->widget_diff_view->updateDiffView(diff, false);
	}
}

void FileHistoryWindow::on_tableWidget_log_currentItemChanged(QTableWidgetItem * /*current*/, QTableWidgetItem * /*previous*/)
{
	updateDiffView();
}

void FileHistoryWindow::on_tableWidget_log_customContextMenuRequested(const QPoint &pos)
{
	(void)pos;
	GitCommitItem commit;
	int row = ui->tableWidget_log->currentRow();
	if (row >= 0 && row < (int)m->commit_item_list.size()) {
		commit = m->commit_item_list[row];
	}
	if (!commit) return;

	QMenu menu;
	QAction *a_property = mainwindow()->addMenuActionProperty(&menu);
	QAction *a = menu.exec(QCursor::pos() + QPoint(8, -8));
	if (a) {
		if (a == a_property) {
			mainwindow()->execCommitPropertyDialog(this, commit);
			return;
		}
	}
}
