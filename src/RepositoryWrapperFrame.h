#ifndef REPOSITORYWRAPPERFRAME_H
#define REPOSITORYWRAPPERFRAME_H

#include "BranchLabel.h"
#include "Git.h"

#include <QFrame>

class MainWindow;
class LogTableWidget;
class FilesListWidget;
class FileDiffWidget;

class RepositoryWrapperFrame : public QFrame {
	Q_OBJECT
	friend class MainWindow;
private:
	Git::CommitItemList logs;

	MainWindow *mw_ = nullptr;
	LogTableWidget *logtablewidget_ = nullptr;
	FilesListWidget *fileslistwidget_ = nullptr;
	FilesListWidget *unstagedfileslistwidget_ = nullptr;
	FilesListWidget *stagesfileslistwidget_ = nullptr;
	FileDiffWidget *filediffwidget_ = nullptr;
	MainWindow *mainwindow();
	MainWindow const *mainwindow() const;
public:
	explicit RepositoryWrapperFrame(QWidget *parent = nullptr);
	Git::CommitItem const *commitItem(int row);
	QIcon verifiedIcon(char s) const;
	QIcon committerIcon(int row) const;
	const QList<BranchLabel> *label(int row) const;
	QString currentBranchName() const;
	const Git::CommitItemList &getLogs() const;
	bool isAncestorCommit(const QString &id);
	QColor color(unsigned int i);
	void updateAncestorCommitMap();
	void bind(MainWindow *mw
			  , LogTableWidget *logtablewidget
			  , FilesListWidget *fileslistwidget
			  , FilesListWidget *unstagedfileslistwidget
			  , FilesListWidget *stagesfileslistwidget
			  , FileDiffWidget *filediffwidget
			  );

	void prepareLogTableWidget();
	void clearLogContents();
	LogTableWidget *logtablewidget();
	FilesListWidget *fileslistwidget();
	FilesListWidget *unstagedFileslistwidget();
	FileDiffWidget *filediffwidget();
	FilesListWidget *stagedFileslistwidget();
	void updateLogTableView();
	void setFocusToLogTable();
	void selectLogTableRow(int row);
};

struct RepositoryWrapperFrameP {
	RepositoryWrapperFrame *pointer;
	RepositoryWrapperFrameP(RepositoryWrapperFrame *pointer = nullptr)
		: pointer(pointer)
	{
	}
};

#endif // REPOSITORYWRAPPERFRAME_H
