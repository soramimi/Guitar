#include "FileHistoryWindow.h"
#include "MainWindow.h"
#include "ui_FileHistoryWindow.h"
#include "misc.h"
#include "GitDiff.h"
#include "joinpath.h"
#include "FileDiffWidget.h"

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
	return &pv->diff_data;
}

const FileDiffWidget::DiffData *FileHistoryWindow::diffdata() const
{
	return &pv->diff_data;
}

FileDiffWidget::DrawData *FileHistoryWindow::drawdata()
{
	return &pv->draw_data;
}

const FileDiffWidget::DrawData *FileHistoryWindow::drawdata() const
{
	return &pv->draw_data;
}

int FileHistoryWindow::totalTextLines() const
{
	return diffdata()->left_lines.size();
}

int FileHistoryWindow::fileviewScrollPos() const
{
	return drawdata()->v_scroll_pos;
}

FileHistoryWindow::FileHistoryWindow(QWidget *parent, GitPtr g, const QString &path)
	: QDialog(parent)
	, ui(new Ui::FileHistoryWindow)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	pv = new Private();

	pv->mainwindow = qobject_cast<MainWindow *>(parent);
	Q_ASSERT(pv->mainwindow);

	ui->splitter->setSizes({100, 100});

	ui->widget_diff_view->bind(pv->mainwindow);

	QString reponame = pv->mainwindow->currentRepositoryName();
	QString brname = pv->mainwindow->currentBranch().name;

	QString text = "%1 (%2)";
	text = text.arg(reponame).arg(brname);
	ui->label_repo->setText(text);
	ui->label_path->setText(path);

	Q_ASSERT(g);
	Q_ASSERT(g->isValidWorkingCopy());
	this->pv->g = g;
	this->pv->path = path;


	collectFileHistory();

	updateDiffView();
}

FileHistoryWindow::~FileHistoryWindow()
{
	delete pv;
	delete ui;
}

void FileHistoryWindow::collectFileHistory()
{
	pv->commit_item_list = pv->g->log_all(pv->path, pv->mainwindow->limitLogCount(), pv->mainwindow->limitLogTime());

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

	int count = pv->commit_item_list.size();
	ui->tableWidget_log->setRowCount(count);

	for (int row = 0; row < count; row++) {
		Git::CommitItem const &commit = pv->commit_item_list[row];
		int col = 0;
		auto AddColumn = [&](QString const &text){
			QTableWidgetItem *item = new QTableWidgetItem(text);
			ui->tableWidget_log->setItem(row, col, item);
			col++;
		};

		QString commit_id = pv->mainwindow->abbrevCommitID(commit);
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
	ui->widget_diff_view->clearDiffView();

	int row = ui->tableWidget_log->currentRow();
	if (row >= 0 && row + 1 < (int)pv->commit_item_list.size()) {
		Git::CommitItem const &commit_left = pv->commit_item_list[row + 1]; // older
		Git::CommitItem const &commit_right = pv->commit_item_list[row];    // newer

		FindFileIdThread left_thread(pv->mainwindow, pv->g->dup(), commit_left.commit_id, pv->path);
		FindFileIdThread right_thread(pv->mainwindow, pv->g->dup(), commit_right.commit_id, pv->path);
		left_thread.start();
		right_thread.start();
		left_thread.wait();
		right_thread.wait();
		QString id_left = left_thread.result;
		QString id_right = right_thread.result;

		ui->widget_diff_view->updateDiffView(id_left, id_right);
	} else if (row >= 0 && row < (int)pv->commit_item_list.size()) {
		Git::CommitItem const &commit = pv->commit_item_list[row];    // newer
		QString id = pv->mainwindow->findFileID(pv->g, commit.commit_id, pv->path);

		Git::Diff diff(id, pv->path, QString());
		ui->widget_diff_view->updateDiffView(diff, false);
	}
}

void FileHistoryWindow::on_tableWidget_log_currentItemChanged(QTableWidgetItem * /*current*/, QTableWidgetItem * /*previous*/)
{
	updateDiffView();
}

