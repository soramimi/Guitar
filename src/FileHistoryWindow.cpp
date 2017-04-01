#include "FileHistoryWindow.h"
#include "MainWindow.h"
#include "ui_FileHistoryWindow.h"
#include "misc.h"
#include "GitDiff.h"
#include "joinpath.h"
#include "FileDiffWidget.h"
#include "MyTableWidgetDelegate.h"

#include <QPainter>
#include <QStyledItemDelegate>
#include <QThread>

struct FileHistoryWindow::Private {
	MainWindow *mainwindow;
	GitPtr g;
	QString path;
	Git::CommitItemList commit_item_list;
	FileDiffWidget::DiffData diff_data;
	FileDiffWidget::DrawData draw_data;
};

FileDiffWidget::DiffData *FileHistoryWindow::diffdata()
{
	return &m->diff_data;
}

const FileDiffWidget::DiffData *FileHistoryWindow::diffdata() const
{
	return &m->diff_data;
}

FileDiffWidget::DrawData *FileHistoryWindow::drawdata()
{
	return &m->draw_data;
}

const FileDiffWidget::DrawData *FileHistoryWindow::drawdata() const
{
	return &m->draw_data;
}

int FileHistoryWindow::totalTextLines() const
{
	return diffdata()->left->lines.size();
}

int FileHistoryWindow::fileviewScrollPos() const
{
	return drawdata()->v_scroll_pos;
}

FileHistoryWindow::FileHistoryWindow(QWidget *parent)
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

	m->mainwindow = qobject_cast<MainWindow *>(parent);
	Q_ASSERT(m->mainwindow);

	ui->splitter->setSizes({100, 200});

	ui->widget_diff_view->bind(m->mainwindow);

	connect(ui->widget_diff_view, SIGNAL(moveNextItem()), this, SLOT(onMoveNextItem()));
	connect(ui->widget_diff_view, SIGNAL(movePreviousItem()), this, SLOT(onMovePreviousItem()));
}

FileHistoryWindow::~FileHistoryWindow()
{
	delete m;
	delete ui;
}

void FileHistoryWindow::prepare(GitPtr g, const QString &path)
{
	Q_ASSERT(g);
	Q_ASSERT(g->isValidWorkingCopy());
	this->m->g = g;
	this->m->path = path;

	QString reponame = m->mainwindow->currentRepositoryName();
	QString brname = m->mainwindow->currentBranch().name;

	QString text = "%1 (%2)";
	text = text.arg(reponame).arg(brname);
	ui->label_repo->setText(text);
	ui->label_path->setText(path);

	{
		OverrideWaitCursor;
		m->commit_item_list = m->g->log_all(m->path, m->mainwindow->limitLogCount());
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

	int count = m->commit_item_list.size();
	ui->tableWidget_log->setRowCount(count);

	for (int row = 0; row < count; row++) {
		Git::CommitItem const &commit = m->commit_item_list[row];
		int col = 0;
		auto AddColumn = [&](QString const &text, QString const &tooltip){
			QTableWidgetItem *item = new QTableWidgetItem(text);
			item->setToolTip(tooltip);
			ui->tableWidget_log->setItem(row, col, item);
			col++;
		};

		QString commit_id = m->mainwindow->abbrevCommitID(commit);
		QString datetime = misc::makeDateTimeString(commit.commit_date);
		AddColumn(commit_id, QString());
		AddColumn(datetime, QString());
		AddColumn(commit.author, QString());
		AddColumn(commit.message, commit.message);
		ui->tableWidget_log->setRowHeight(row, 24);
	}

	ui->tableWidget_log->resizeColumnsToContents();
	ui->tableWidget_log->horizontalHeader()->setStretchLastSection(false);
	ui->tableWidget_log->horizontalHeader()->setStretchLastSection(true);

	ui->tableWidget_log->setFocus();
	ui->tableWidget_log->setCurrentCell(0, 0);
}

class FindFileIdThread : public QThread {
private:
	MainWindow *mainwindow;
	GitPtr g;
	QString commit_id;
	QString file;
public:
	QString result;
	FindFileIdThread(MainWindow *mainwindow, GitPtr g, const QString &commit_id, const QString &file)
	{
		this->mainwindow = mainwindow;
		this->g = g;
		this->commit_id = commit_id;
		this->file = file;
	}

protected:
	void run()
	{
		result = mainwindow->findFileID(g, commit_id, file);
	}
};

void FileHistoryWindow::updateDiffView()
{
	Q_ASSERT(m->g);
	Q_ASSERT(m->g->isValidWorkingCopy());

	ui->widget_diff_view->clearDiffView();

	int row = ui->tableWidget_log->currentRow();
	if (row >= 0 && row + 1 < (int)m->commit_item_list.size()) {
		Git::CommitItem const &commit_left = m->commit_item_list[row + 1]; // older
		Git::CommitItem const &commit_right = m->commit_item_list[row];    // newer

		FindFileIdThread left_thread(m->mainwindow, m->g->dup(), commit_left.commit_id, m->path);
		FindFileIdThread right_thread(m->mainwindow, m->g->dup(), commit_right.commit_id, m->path);
		left_thread.start();
		right_thread.start();
		left_thread.wait();
		right_thread.wait();
		QString id_left = left_thread.result;
		QString id_right = right_thread.result;

		ui->widget_diff_view->updateDiffView(id_left, id_right);
	} else if (row >= 0 && row < (int)m->commit_item_list.size()) {
		Git::CommitItem const &commit = m->commit_item_list[row];    // newer
		QString id = m->mainwindow->findFileID(m->g, commit.commit_id, m->path);

		Git::Diff diff(id, m->path, QString());
		ui->widget_diff_view->updateDiffView(diff, false);
	}
}

void FileHistoryWindow::on_tableWidget_log_currentItemChanged(QTableWidgetItem * /*current*/, QTableWidgetItem * /*previous*/)
{
	updateDiffView();
}

void FileHistoryWindow::onMoveNextItem()
{
	int row = ui->tableWidget_log->currentRow();
	int count = ui->tableWidget_log->rowCount();
	if (row + 1 < count) {
		ui->tableWidget_log->setCurrentCell(row + 1, 0, QItemSelectionModel::ClearAndSelect);
	}
}

void FileHistoryWindow::onMovePreviousItem()
{
	int row = ui->tableWidget_log->currentRow();
	if (row > 0) {
		ui->tableWidget_log->setCurrentCell(row - 1, 0, QItemSelectionModel::ClearAndSelect);
	}
}

