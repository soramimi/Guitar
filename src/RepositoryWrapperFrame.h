#ifndef REPOSITORYWRAPPERFRAME_H
#define REPOSITORYWRAPPERFRAME_H

#include "BranchLabel.h"
#include "Git.h"
#include "GitObjectManager.h"

#include <QFrame>

class MainWindow;
class LogTableWidget;
class FilesListWidget;
class FileDiffWidget;

class RepositoryWrapperFrame : public QFrame {
	Q_OBJECT
	friend class MainWindow;
private:
	Git::CommitItemList commit_log;

	MainWindow *mw_ = nullptr;
	LogTableWidget *logtablewidget_ = nullptr;
	FilesListWidget *fileslistwidget_ = nullptr;
	FilesListWidget *unstagedfileslistwidget_ = nullptr;
	FilesListWidget *stagesfileslistwidget_ = nullptr;
	FileDiffWidget *filediffwidget_ = nullptr;

	std::map<QString, QList<Git::Branch>> branch_map;
	std::map<QString, QList<Git::Tag>> tag_map;
	std::map<int, QList<BranchLabel>> label_map;
	std::map<QString, Git::Diff> diff_cache;

	GitObjectCache objcache;

	MainWindow *mainwindow();
	MainWindow const *mainwindow() const;
protected:
	void customEvent(QEvent *e);
public:
	explicit RepositoryWrapperFrame(QWidget *parent = nullptr);
	Git::CommitItem const *commitItem(int row);
	QIcon verifiedIcon(char s) const;
	QImage committerIcon(int row, QSize size) const;
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
