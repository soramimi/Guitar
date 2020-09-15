#include "LogTableWidget.h"
#include "MainWindow.h"
#include "RepositoryWrapperFrame.h"

RepositoryWrapperFrame::RepositoryWrapperFrame(QWidget *parent)
	: QFrame(parent)
{

}

void RepositoryWrapperFrame::bind(MainWindow *mw, LogTableWidget *logtablewidget, FilesListWidget *fileslistwidget, FilesListWidget *unstagedfileslistwidget, FilesListWidget *stagesfileslistwidget, FileDiffWidget *filediffwidget)
{
	mw_ = mw;
	logtablewidget_ = logtablewidget;
	fileslistwidget_ = fileslistwidget;
	unstagedfileslistwidget_ = unstagedfileslistwidget;
	stagesfileslistwidget_ = stagesfileslistwidget;
	filediffwidget_ = filediffwidget;
	logtablewidget->bind(this);
}

MainWindow *RepositoryWrapperFrame::mainwindow()
{
	Q_ASSERT(mw_);
	return mw_;
}

MainWindow const *RepositoryWrapperFrame::mainwindow() const
{
	Q_ASSERT(mw_);
	return mw_;
}

LogTableWidget *RepositoryWrapperFrame::logtablewidget()
{
	return logtablewidget_;
}

FilesListWidget *RepositoryWrapperFrame::fileslistwidget()
{
	return fileslistwidget_;
}

FilesListWidget *RepositoryWrapperFrame::unstagedFileslistwidget()
{
	return unstagedfileslistwidget_;
}

FilesListWidget *RepositoryWrapperFrame::stagedFileslistwidget()
{
	return stagesfileslistwidget_;
}

FileDiffWidget *RepositoryWrapperFrame::filediffwidget()
{
	return filediffwidget_;
}

const Git::CommitItem *RepositoryWrapperFrame::commitItem(int row)
{
	return mainwindow()->commitItem(mainwindow()->frame(), row);
}

QIcon RepositoryWrapperFrame::verifiedIcon(char s) const
{
	return mainwindow()->verifiedIcon(s);
}

QIcon RepositoryWrapperFrame::committerIcon(int row) const
{
	return mainwindow()->committerIcon(const_cast<RepositoryWrapperFrame *>(this), row);
}

QList<BranchLabel> const *RepositoryWrapperFrame::label(int row) const
{
	return mainwindow()->label(this, row);
}

QString RepositoryWrapperFrame::currentBranchName() const
{
	return mainwindow()->currentBranchName();
}

const Git::CommitItemList &RepositoryWrapperFrame::getLogs() const
{
	return mainwindow()->getLogs(this);
}

bool RepositoryWrapperFrame::isAncestorCommit(const QString &id)
{
	return mainwindow()->isAncestorCommit(id);
}

QColor RepositoryWrapperFrame::color(unsigned int i)
{
	return mainwindow()->color(i);
}

void RepositoryWrapperFrame::updateAncestorCommitMap()
{
	mainwindow()->updateAncestorCommitMap(this);
}

void RepositoryWrapperFrame::updateLogTableView()
{
	logtablewidget_->viewport()->update();
}

void RepositoryWrapperFrame::setFocusToLogTable()
{
	logtablewidget_->setFocus();
}

void RepositoryWrapperFrame::selectLogTableRow(int row)
{
	logtablewidget_->selectRow(row);
}

void RepositoryWrapperFrame::prepareLogTableWidget()
{
	QStringList cols = {
		tr("Graph"),
		tr("Commit"),
		tr("Date"),
		tr("Author"),
		tr("Message"),
	};
	int n = cols.size();
	logtablewidget_->setColumnCount(n);
	logtablewidget_->setRowCount(0);
	for (int i = 0; i < n; i++) {
		QString const &text = cols[i];
		auto *item = new QTableWidgetItem(text);
		logtablewidget_->setHorizontalHeaderItem(i, item);
	}

	mainwindow()->updateCommitGraph(this); // コミットグラフを更新
}

void RepositoryWrapperFrame::clearLogContents()
{
	logtablewidget_->clearContents();
	logtablewidget_->scrollToTop();
}
