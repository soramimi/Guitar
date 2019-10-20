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
	explicit MainWindow(QWidget *parent = nullptr);
	~MainWindow() override;

	QPixmap const &digitsPixmap() const;

	QString currentWorkingCopyDir() const override;

	QColor color(unsigned int i);

	bool isOnlineMode() const override;
private:
	Ui::MainWindow *ui;

	void updateFilesList(QString id, bool wait) override;
	void updateFilesList(Git::CommitItem const &commit, bool wait);
	void updateRepositoriesList() override;

	void openRepository_(GitPtr g, bool keep_selection = false) override;

	void prepareLogTableWidget();
	QStringList selectedFiles_(QListWidget *listwidget) const;
	QStringList selectedFiles() const;
	void for_each_selected_files(std::function<void (QString const &)> const &fn);
	void showFileList(FilesListType files_list_type);

	void clearLog();
	void clearFileList() override;
	void clearDiffView();
	void clearRepositoryInfo();

	int repositoryIndex_(const QTreeWidgetItem *item) const;
	RepositoryItem const *repositoryItem(const QTreeWidgetItem *item) const;

	QTreeWidgetItem *newQTreeWidgetFolderItem(QString const &name);
	void buildRepoTree(QString const &group, QTreeWidgetItem *item, QList<RepositoryItem> *repos);
	void refrectRepositories();

	void updateDiffView(QListWidgetItem *item);
	void updateDiffView();
	void updateUnstagedFileCurrentItem();
	void updateStagedFileCurrentItem();
	void updateStatusBarText() override;
	void setRepositoryInfo(QString const &reponame, QString const &brname) override;
	int indexOfRepository(const QTreeWidgetItem *treeitem) const;
	void clearRepoFilter();
	void appendCharToRepoFilter(ushort c);
	void backspaceRepoFilter();
	void revertCommit();
	void mergeBranch(const QString &commit, Git::MergeFastForward ff);
	void mergeBranch(Git::CommitItem const *commit, Git::MergeFastForward ff);
	void rebaseBranch(Git::CommitItem const *commit);
	void cherrypick(Git::CommitItem const *commit);
	void merge(const Git::CommitItem *commit = nullptr);
	void detectGitServerType(const GitPtr &g);
	void setRemoteOnline(bool f, bool save);
	void startTimers();
	void onCloneCompleted(bool success, const QVariant &userdata);
	void setNetworkingCommandsEnabled(bool enabled);
	void blame(QListWidgetItem *item);
	void blame();
	QListWidgetItem *currentFileItem() const;
	void execAreYouSureYouWantToContinueConnectingDialog();
	void deleteRemoteBranch(Git::CommitItem const *commit);
	QStringList remoteBranches(QString const &id, QStringList *all);
	bool isUninitialized();
	void doLogCurrentItemChanged();
	void findNext();
	void findText(const QString &text);
	void showStatus();
	void onStartEvent();
	void showLogWindow(bool show);
protected:
	void customEvent(QEvent *);
	void dragEnterEvent(QDragEnterEvent *event) override;
	void timerEvent(QTimerEvent *) override;
	void keyPressEvent(QKeyEvent *event) override;
	bool event(QEvent *event) override;
	bool eventFilter(QObject *watched, QEvent *event) override;
public:
	void drawDigit(QPainter *pr, int x, int y, int n) const;
	int digitWidth() const;
	int digitHeight() const;
	void setStatusBarText(QString const &text);
	void clearStatusBarText();
	void setCurrentLogRow(int row) override;
	bool shown();
	void deleteTags(QStringList const &tagnames) override;
	bool addTag(QString const &name);
	void updateCurrentFilesList();
	void notifyRemoteChanged(bool f);
	void postOpenRepositoryFromGitHub(const QString &username, const QString &reponame);
	int selectedLogIndex() const override;
	void updateAncestorCommitMap();
	bool isAncestorCommit(const QString &id);
	void test();
	void postStartEvent();
private slots:
	void updateUI();
	void onLogVisibilityChanged();
	void onPtyProcessCompleted(bool ok, const QVariant &userdata);
	void onRepositoriesTreeDropped();
	void on_action_about_triggered();
	void on_action_clean_df_triggered();
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
	void on_action_explorer_triggered();
	void on_action_fetch_triggered();
	void on_action_fetch_prune_triggered();
	void on_action_find_next_triggered();
	void on_action_find_triggered();
	void on_action_offline_triggered();
	void on_action_online_triggered();
	void on_action_open_existing_working_copy_triggered();
	void on_action_pull_triggered();
	void on_action_push_all_tags_triggered();
	void on_action_push_triggered();
	void on_action_push_u_triggered();
	void on_action_reflog_triggered();
	void on_action_repo_checkout_triggered();
	void on_action_repo_jump_to_head_triggered();
	void on_action_repo_jump_triggered();
	void on_action_repositories_panel_triggered();
	void on_action_repository_property_triggered();
	void on_action_repository_status_triggered();
	void on_action_reset_HEAD_1_triggered();
	void on_action_reset_hard_triggered();
	void on_action_set_config_user_triggered();
	void on_action_set_gpg_signing_triggered();
	void on_action_stash_apply_triggered();
	void on_action_stash_drop_triggered();
	void on_action_stash_triggered();
	void on_action_stop_process_triggered();
	void on_action_terminal_triggered();
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
	void on_toolButton_status_clicked();
	void on_toolButton_stop_process_clicked();
	void on_toolButton_terminal_clicked();
	void on_toolButton_unstage_clicked();
	void on_treeWidget_repos_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
	void on_treeWidget_repos_customContextMenuRequested(const QPoint &pos);
	void on_treeWidget_repos_itemDoubleClicked(QTreeWidgetItem *item, int column);
	void on_verticalScrollBar_log_valueChanged(int);
	void on_action_repo_merge_triggered();

	void on_action_expand_commit_log_triggered();

	void on_action_expand_file_list_triggered();

	void on_action_expand_diff_view_triggered();

	void on_action_wide_triggered();

	void on_action_sidebar_triggered();


protected:
	void closeEvent(QCloseEvent *event) override;
	void internalWriteLog(const char *ptr, int len) override;
	RepositoryItem const *selectedRepositoryItem() const override;
	void removeSelectedRepositoryFromBookmark(bool ask) override;
protected slots:
	void onLogIdle();
signals:
	void signalSetRemoteChanged(bool f);
	void onEscapeKeyPressed();
	void updateButton();
};

#endif // MAINWINDOW_H
