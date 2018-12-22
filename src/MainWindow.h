#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "BasicMainWindow.h"

namespace Ui {
class MainWindow;
}

class QTableWidgetItem;

class HunkItem {
public:
	int hunk_number = -1;
	size_t pos, len;
	std::vector<std::string> lines;
};

class MainWindow : public BasicMainWindow {
	Q_OBJECT
	friend class ImageViewWidget;
	friend class FileDiffSliderWidget;
	friend class FileHistoryWindow;
	friend class FileDiffWidget;
	friend class AboutDialog;
private:
	struct Private;
	Private *m;
public:
	explicit MainWindow(QWidget *parent = 0);
	~MainWindow();

	QPixmap const &digitsPixmap() const;

	QString currentWorkingCopyDir() const;

	QColor color(unsigned int i);
private:
	Ui::MainWindow *ui;

	void updateFilesList(QString id, bool wait);
	void updateFilesList(const Git::CommitItem &commit, bool wait);
	void updateRepositoriesList();

	void openRepository_(GitPtr g);

	void prepareLogTableWidget();
	QStringList selectedFiles_(QListWidget *listwidget) const;
	QStringList selectedFiles() const;
	void for_each_selected_files(std::function<void (QString const &)> fn);
	void updateCommitGraph();
	void showFileList(FilesListType files_list_type);

	void clearLog();
	void clearFileList();
	void clearDiffView();
	void clearRepositoryInfo();

	int repositoryIndex_(const QTreeWidgetItem *item) const;
	RepositoryItem const *repositoryItem(const QTreeWidgetItem *item) const;

	int selectedLogIndex() const;
	QTreeWidgetItem *newQTreeWidgetFolderItem(QString const &name);
	void buildRepoTree(QString const &group, QTreeWidgetItem *item, QList<RepositoryItem> *repos);
	void refrectRepositories();

	void updateDiffView(QListWidgetItem *item);
	void updateDiffView();
	void updateUnstagedFileCurrentItem();
	void updateStagedFileCurrentItem();
	void updateStatusBarText();
	void setRepositoryInfo(QString const &reponame, QString const &brname);
	int indexOfRepository(const QTreeWidgetItem *treeitem) const;
	void clearRepoFilter();
	void appendCharToRepoFilter(ushort c);
	void backspaceRepoFilter();
	void revertCommit();
	void cherrypick(const Git::CommitItem *commit);
	void mergeBranch(const Git::CommitItem *commit);
	void rebaseBranch(const Git::CommitItem *commit);
	void detectGitServerType(GitPtr g);
	void setRemoteOnline(bool f);
	bool isRemoteOnline() const;
	void startTimers();
	void onCloneCompleted(bool success);
	bool fetch(GitPtr g);
	void setNetworkingCommandsEnabled(bool f);
	void blame(QListWidgetItem *item);
	void blame();
	QListWidgetItem *currentFileItem() const;
	void execAreYouSureYouWantToContinueConnectingDialog();
	void deleteRemoteBranch(const Git::CommitItem *commit);
	QStringList remoteBranches(QString const &id);
	void rebaseOnto();
	void setWatchRemoteInterval(int mins);
protected:
	void dragEnterEvent(QDragEnterEvent *event);
	void timerEvent(QTimerEvent *);
	void keyPressEvent(QKeyEvent *event);
	bool event(QEvent *event);
	bool eventFilter(QObject *watched, QEvent *event);
public:
	void drawDigit(QPainter *pr, int x, int y, int n) const;
	int digitWidth() const;
	int digitHeight() const;
	void setStatusBarText(QString const &text);
	void clearStatusBarText();
	void setCurrentLogRow(int row);
	bool shown();
	void deleteTags(QStringList const &tagnames);
	bool addTag(QString const &name);
	void updateCurrentFilesList();
public slots:
	void setRemoteChanged(bool f);
private slots:
	void doUpdateButton();
	void onLogVisibilityChanged();
	void onPtyProcessCompleted();
	void onRepositoriesTreeDropped();
	void on_action_about_triggered();
	void on_action_clone_triggered();
	void on_action_commit_triggered();
	void on_action_create_a_repository_triggered();
	void on_action_delete_branch_triggered();
	void on_action_delete_remote_branch_triggered();
	void on_action_edit_git_config_triggered();
	void on_action_edit_gitignore_triggered();
	void on_action_edit_global_gitconfig_triggered();
	void on_action_edit_settings_triggered();
	void on_action_edit_tags_triggered();
	void on_action_exit_triggered();
	void on_action_fetch_triggered();
	void on_action_open_existing_working_copy_triggered();
	void on_action_pull_triggered();
	void on_action_push_triggered();
	void on_action_rebase_onto_triggered();
	void on_action_reflog_triggered();
	void on_action_repo_checkout_triggered();
	void on_action_repo_jump_triggered();
	void on_action_repository_property_triggered();
	void on_action_reset_HEAD_1_triggered();
	void on_action_set_config_user_triggered();
	void on_action_set_gpg_signing_triggered();
	void on_action_stop_process_triggered();
	void on_action_tag_push_all_triggered();
	void on_action_test_triggered();
	void on_action_view_refresh_triggered();
	void on_action_window_log_triggered(bool checked);
	void on_horizontalScrollBar_log_valueChanged(int);
	void on_lineEdit_filter_textChanged(QString const &text);
	void on_listWidget_files_currentRowChanged(int currentRow);
	void on_listWidget_files_customContextMenuRequested(const QPoint &pos);
	void on_listWidget_files_itemDoubleClicked(QListWidgetItem *item);
	void on_listWidget_staged_currentRowChanged(int currentRow);
	void on_listWidget_staged_customContextMenuRequested(const QPoint &pos);
	void on_listWidget_staged_itemDoubleClicked(QListWidgetItem *item);
	void on_listWidget_unstaged_currentRowChanged(int currentRow);
	void on_listWidget_unstaged_customContextMenuRequested(const QPoint &pos);
	void on_listWidget_unstaged_itemDoubleClicked(QListWidgetItem *item);
	void on_radioButton_remote_offline_clicked();
	void on_radioButton_remote_online_clicked();
	void on_tableWidget_log_currentItemChanged(QTableWidgetItem *current, QTableWidgetItem *previous);
	void on_tableWidget_log_customContextMenuRequested(const QPoint &pos);
	void on_tableWidget_log_itemDoubleClicked(QTableWidgetItem *);
	void on_toolButton_clone_clicked();
	void on_toolButton_commit_clicked();
	void on_toolButton_erase_filter_clicked();
	void on_toolButton_explorer_clicked();
	void on_toolButton_fetch_clicked();
	void on_toolButton_pull_clicked();
	void on_toolButton_push_clicked();
	void on_toolButton_select_all_clicked();
	void on_toolButton_stage_clicked();
	void on_toolButton_stop_process_clicked();
	void on_toolButton_terminal_clicked();
	void on_toolButton_unstage_clicked();
	void on_treeWidget_repos_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
	void on_treeWidget_repos_customContextMenuRequested(const QPoint &pos);
	void on_treeWidget_repos_itemDoubleClicked(QTreeWidgetItem *item, int column);
	void on_verticalScrollBar_log_valueChanged(int);
	void on_action_push_u_triggered();
protected:
	void closeEvent(QCloseEvent *event);
	virtual void internalWriteLog(const char *ptr, int len);
	const RepositoryItem *selectedRepositoryItem() const;
	void removeSelectedRepositoryFromBookmark(bool ask);
protected slots:
	void onLogIdle();
signals:
	void signalSetRemoteChanged(bool f);
	void onEscapeKeyPressed();
	void updateButton();
};

#endif // MAINWINDOW_H
