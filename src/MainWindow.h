#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "Git.h"
#include "RepositoryData.h"
#include <memory>
#include <functional>

namespace Ui {
class MainWindow;
}

class QScrollBar;

class QListWidgetItem;
class QTreeWidgetItem;
class QTableWidgetItem;

class CommitList;

#define PATH_PREFIX "*"

class HunkItem {
public:
	int hunk_number = -1;
	size_t pos, len;
	QStringList lines;
};

class MainWindow : public QMainWindow
{
	Q_OBJECT
	friend class FilePreviewWidget;
	friend class FileDiffSliderWidget;
	friend class FileHistoryWindow;
	friend class FileDiffWidget;
private:

	struct Private;
	Private *pv;
public:
	explicit MainWindow(QWidget *parent = 0);
	~MainWindow();


	QPixmap const &digitsPixmap() const;

	QString currentWorkingCopyDir() const;

	static QString makeRepositoryName(QString const &loc);
	const Git::CommitItemList *logs() const;
	QColor color(unsigned int i);
private slots:
	void on_action_add_all_triggered();
	void on_action_branch_checkout_triggered();
	void on_action_branch_merge_triggered();
	void on_action_branch_new_triggered();
	void on_action_clone_triggered();
	void on_action_commit_triggered();
	void on_action_config_global_credential_helper_triggered();
	void on_action_edit_git_config_triggered();
	void on_action_edit_gitignore_triggered();
	void on_action_edit_global_gitconfig_triggered();
	void on_action_edit_settings_triggered();
	void on_action_fetch_triggered();
	void on_action_open_existing_working_copy_triggered();
	void on_action_pull_triggered();
	void on_action_push_triggered();
	void on_action_set_remote_origin_url_triggered();
	void on_action_test_triggered();
	void on_action_view_refresh_triggered();
	void on_tableWidget_log_currentItemChanged(QTableWidgetItem *current, QTableWidgetItem *previous);
	void on_treeWidget_repos_itemDoubleClicked(QTreeWidgetItem *item, int column);

	void on_listWidget_files_customContextMenuRequested(const QPoint &pos);
	void on_listWidget_unstaged_customContextMenuRequested(const QPoint &pos);
	void on_listWidget_staged_customContextMenuRequested(const QPoint &pos);

	void on_listWidget_unstaged_currentRowChanged(int currentRow);
	void on_listWidget_staged_currentRowChanged(int currentRow);
	void on_listWidget_files_currentRowChanged(int currentRow);
	void on_toolButton_commit_clicked();
	void on_toolButton_pull_clicked();
	void on_toolButton_push_clicked();
	void on_toolButton_select_all_clicked();
	void on_toolButton_stage_clicked();
	void on_toolButton_unstage_clicked();
	void on_action_about_triggered();
	void on_toolButton_clone_clicked();
	void on_toolButton_fetch_clicked();
	void on_comboBox_filter_currentTextChanged(const QString &arg1);
	void on_toolButton_erase_filter_clicked();
	void on_tableWidget_log_customContextMenuRequested(const QPoint &pos);
	void on_action_tag_triggered();

	void on_action_tag_push_all_triggered();

	void on_action_tag_delete_triggered();

	void on_treeWidget_repos_customContextMenuRequested(const QPoint &pos);


	void onRepositoriesTreeDropped();


private:
	Ui::MainWindow *ui;

	enum class FilesListType {
		SingleList,
		SideBySide,
	};
	void updateFilesList(QString const &new_id, FilesListType files_list_type);
	void updateHeadFilesList(bool wait);
	void updateRepositoriesList();
	QString getBookmarksFilePath() const;
	bool saveRepositoryBookmarks() const;

	GitPtr git(const QString &dir);
	GitPtr git();

	void openRepository_(GitPtr g);
	void openRepository(bool waitcursor = true);
	void reopenRepository(std::function<void(GitPtr g)> callback);

	void openSelectedRepository();
	bool askAreYouSureYouWantToRun(const QString &title, const QString &command);
	void revertFile(const QStringList &path);
	void revertAllFiles();
	void prepareLogTableWidget();
	QStringList selectedStagedFiles() const;
	QStringList selectedUnstagedFiles() const;
	void for_each_selected_staged_files(std::function<void (const QString &)> fn);
	void for_each_selected_unstaged_files(std::function<void (const QString &)> fn);
	bool editFile(const QString &path, const QString &title);
	void updateCommitGraph();
	void updateCurrentFilesList();
	void checkGitCommand();
	void showFileList(FilesListType files_list_type);

	void clearLog();
	void clearFileList();
	void clearDiffView();
	void clearRepositoryInfo();

	int repositoryIndex_(QTreeWidgetItem *item);
	RepositoryItem const *repositoryItem(QTreeWidgetItem *item);

	void stopDiff();
	void startDiff(GitPtr g, const QString &id);
	bool makeDiff(const QString &id, QList<Git::Diff> *out);

	void udpateButton();
	void commit(bool amend = false);
	void commit_amend();
	void queryBranches(GitPtr g);
	void queryTags(GitPtr g);
	QList<Git::Branch> findBranch(const QString &id);
	QList<Git::Tag> findTag(const QString &id);
	int selectedLogIndex() const;
	const Git::CommitItem *selectedCommitItem() const;
	void deleteTags(const QStringList &tagnames);
	void deleteTags(const Git::CommitItem &commit);
	void deleteSelectedTags();
	QTreeWidgetItem *newQTreeWidgetFolderItem(const QString &name);
	void buildRepoTree(const QString &group, QTreeWidgetItem *item, QList<RepositoryItem> *repos);
	void refrectRepositories();

	bool saveByteArrayAs(const QByteArray &ba, const QString &dstpath);
	bool saveFileAs(const QString &srcpath, const QString &dstpath);
	bool saveBlobAs(const QString &id, const QString &dstpath);
	QString selectCommand_(const QString &cmdname, const QString &cmdfile, QString path, std::function<void(const QString &)> callback);
	void updateDiffView(QListWidgetItem *item);
	void updateUnstagedFileCurrentItem();
	void updateStagedFileCurrentItem();
	void cleanupDiffThread();
	void addTag();
	static bool cat_file(GitPtr g, const QString &id, QByteArray *out);
	void execFileHistory(QListWidgetItem *item);
	void execFileHistory(const QString &path);
public:

	QString selectGitCommand();
	QString selectFileCommand();

	int limitLogCount() const;
	QDateTime limitLogTime() const;

	void setFileCommand(const QString &path, bool save);
	void setGitCommand(const QString &path, bool save);
	QString currentRepositoryName() const;
	Git::Branch currentBranch() const;
	bool isThereUncommitedChanges() const;
	QString defaultWorkingDir() const;
	bool event(QEvent *event);
	bool eventFilter(QObject *watched, QEvent *event);
	void autoOpenRepository(QString dir);
	void saveRepositoryBookmark(RepositoryItem item);
	void drawDigit(QPainter *pr, int x, int y, int n) const;
	int digitWidth() const;
	int digitHeight() const;
	bool isValidWorkingCopy(const GitPtr &g) const;
	QString tempfileHeader() const;
	void deleteTempFiles();
	QString newTempFilePath();
	bool saveAs(const QString &id, const QString &dstpath);
	QString saveAsTemp(const QString &id);
	QString filetype(const QString &path, bool mime);
	QString abbrevCommitID(const Git::CommitItem &commit);
protected:

protected:
	void timerEvent(QTimerEvent *event);
protected slots:

};

#endif // MAINWINDOW_H
