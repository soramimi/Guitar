#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "Git.h"
#include "RepositoryData.h"
#include <memory>
#include <functional>
#include "GitHubAPI.h"
#include "main.h"
#include "TextEditorTheme.h"

namespace Ui {
class MainWindow;
}

class QScrollBar;

class QListWidget;
class QListWidgetItem;
class QTreeWidgetItem;
class QTableWidgetItem;
class AboutDialog;

class MySettings;

class LocalSocketReader;

class CommitList;
class WebContext;

#define PATH_PREFIX "*"


class HunkItem {
public:
	int hunk_number = -1;
	size_t pos, len;
	std::vector<std::string> lines;
};

class MainWindow : public QMainWindow
{
	Q_OBJECT
	friend class ImageViewWidget;
	friend class FileDiffSliderWidget;
	friend class FileHistoryWindow;
	friend class FileDiffWidget;
	friend class AboutDialog;
	friend class SettingDirectoriesForm;
public:
	struct Label {
		enum {
			Head,
			LocalBranch,
			RemoteBranch,
			Tag,
		};
		int kind;
		QString text;
		Label(int kind = LocalBranch)
			: kind(kind)
		{

		}
	};
private:
	struct Private;
	Private *m;
public:
	explicit MainWindow(QWidget *parent = 0);
	~MainWindow();

	QPixmap const &digitsPixmap() const;

	QString currentWorkingCopyDir() const;

	static QString makeRepositoryName(QString const &loc);
	const Git::CommitItemList *logs() const;
	QColor color(unsigned int i);
private:
	Ui::MainWindow *ui;

	enum class FilesListType {
		SingleList,
		SideBySide,
	};

	void updateFilesList(QString id, bool wait);
	void updateFilesList(const Git::CommitItem &commit, bool wait);
	void updateRepositoriesList();
	QString getBookmarksFilePath() const;
	bool saveRepositoryBookmarks() const;

	void openRepository_(GitPtr g);
	void openRepository(bool validate, bool waitcursor = true);
	void reopenRepository(bool log, std::function<void(GitPtr)> callback);

	void openSelectedRepository();
	bool askAreYouSureYouWantToRun(const QString &title, const QString &command);
	void resetFile(const QStringList &path);
	void revertAllFiles();
	void prepareLogTableWidget();
	QStringList selectedFiles_(QListWidget *listwidget) const;
	QStringList selectedFiles() const;
	void for_each_selected_files(std::function<void (const QString &)> fn);
	bool editFile(const QString &path, const QString &title);
	void updateCommitGraph();
	void updateCurrentFilesList();
	void showFileList(FilesListType files_list_type);

	QString selectGitCommand(bool save);
	QString selectFileCommand(bool save);
	bool checkGitCommand();
	bool checkFileCommand();
	void checkUser();

	void clearLog();
	void clearFileList();
	void clearDiffView();
	void clearRepositoryInfo();

	int repositoryIndex_(QTreeWidgetItem *item);
	RepositoryItem const *repositoryItem(QTreeWidgetItem *item);

	bool makeDiff(QString id, QList<Git::Diff> *out);

	void udpateButton();
	void commit(bool amend = false);
	void commit_amend();
	void queryBranches(GitPtr g);
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
	QString selectCommand_(const QString &cmdname, const QString &cmdfile, const QStringList &list, QString path, std::function<void(const QString &)> callback);
	void updateDiffView(QListWidgetItem *item);
	void updateUnstagedFileCurrentItem();
	void updateStagedFileCurrentItem();
	void addTag();
	void execFileHistory(QListWidgetItem *item);
	void execFileHistory(const QString &path);
	QStringList whichCommand_(const QString &cmdfile);
	QString getObjectID(QListWidgetItem *item);
	void execFilePropertyDialog(QListWidgetItem *item);
	static QAction *addMenuActionProperty(QMenu *menu);
	QString determinFileType_(const QString &path, bool mime, std::function<void(QString const &cmd, QByteArray *ba)> callback) const;
	Git::Object cat_file_(GitPtr g, const QString &id);
	Git::Object cat_file(const QString &id);
	void updateStatusBarText();
	void setUnknownRepositoryInfo();
	void setWindowTitle_(const Git::User &user);
	void setRepositoryInfo(const QString &reponame, const QString &brname);
	void updateWindowTitle(GitPtr g);
	void logGitVersion();
	static bool write_log_callback(void *cookie, const char *ptr, int len);
	static bool git_callback(void *cookie, const char *ptr, int len);
	int indexOfRepository(const QTreeWidgetItem *treeitem) const;
	void removeRepositoryFromBookmark(int index, bool ask);

	enum NamedCommitFlag {
		Branches = 0x0001,
		Tags     = 0x0002,
	};
	NamedCommitList namedCommitItems(int flags);

	void deleteBranch(const Git::CommitItem *commit);
	void checkout();
	void clone();
	void deleteBranch();
	Git::CommitItemList retrieveCommitLog(GitPtr g);
	bool runOnCurrentRepositoryDir(std::function<void(QString)> callback);
	void openTerminal();
	void openExplorer();
	void pushSetUpstream(const QString &remote, const QString &branch);
	bool pushSetUpstream(bool testonly);
	void clearRepoFilter();
	void appendCharToRepoFilter(ushort c);
	void backspaceRepoFilter();
	void revertCommit();
	int rowFromCommitId(const QString &id);
	void cherrypick(const Git::CommitItem *commit);
	void mergeBranch(const Git::CommitItem *commit);
	void detectGitServerType(GitPtr g);
	void initNetworking();
	void execSetRemoteUrlDialog(const RepositoryItem *repo = nullptr);
	void setRemoteOnline(bool f);
	bool isRemoteOnline() const;
	void startTimers();
	void onCloneCompleted();
	void fetch(GitPtr g);
	void stopPtyProcess();
	void setNetworkingCommandsEnabled(bool f);
	void execSetUserDialog(const Git::User &global_user, const Git::User &repo_user, const QString &reponame);
	void execSetGlobalUserDialog();
protected:
	void dragEnterEvent(QDragEnterEvent *event);
	void timerEvent(QTimerEvent *);
	void keyPressEvent(QKeyEvent *event);
	bool event(QEvent *event);
	bool eventFilter(QObject *watched, QEvent *event);
public:

	int limitLogCount() const;

	void setFileCommand(const QString &path, bool save);
	void setGitCommand(const QString &path, bool save);
	QString currentRepositoryName() const;
	Git::Branch currentBranch() const;
	bool isThereUncommitedChanges() const;
	QString defaultWorkingDir() const;
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
	QString abbrevCommitID(const Git::CommitItem &commit);
	QString findFileID(GitPtr g, const QString &commit_id, const QString &file);
	QString determinFileType(const QString &path, bool mime);
	QString determinFileType(QByteArray in, bool mime);
	QPixmap getTransparentPixmap();
	QString getCommitIdFromTag(const QString &tag);
	void setStatusBarText(const QString &text);
	void clearStatusBarText();
	QString makeCommitInfoText(int row, QList<Label> *label_list);
	void setLogEnabled(GitPtr g, bool f);
	void addWorkingCopyDir(QString dir, QString name, bool open);
	void addWorkingCopyDir(QString dir, bool open)
	{
		addWorkingCopyDir(dir, QString(), open);
	}
	bool isValidRemoteURL(QString const &url);
	bool testRemoteRepositoryValidity(const QString &url);
	void removeSelectedRepositoryFromBookmark(bool ask);
	void setCurrentLogRow(int row);
	GitPtr git(const QString &dir) const;
	GitPtr git();
	const QList<Label> *label(int row);
	bool isGitHub() const;
	QIcon committerIcon(int row);
	void updateCommitTableLater();
	bool isAvatarEnabled() const;
	WebContext *getWebContextPtr();
	void shown();
	void execCommitExploreWindow(QWidget *parent, const Git::CommitItem *commit);
	void execCommitPropertyDialog(QWidget *parent, const Git::CommitItem *commit);
	void checkout(QWidget *parent, const Git::CommitItem *commit);
	TextEditorThemePtr themeForTextEditor();
public slots:
	void writeLog(const char *ptr, int len);
	void writeLog(const QString &str);
	void writeLog(QByteArray ba);
private slots:
	void on_action_add_all_triggered();
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
	void on_action_tag_triggered();
	void on_action_tag_push_all_triggered();
	void on_action_tag_delete_triggered();

	void on_treeWidget_repos_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
	void on_treeWidget_repos_itemDoubleClicked(QTreeWidgetItem *item, int column);

	void on_tableWidget_log_currentItemChanged(QTableWidgetItem *current, QTableWidgetItem *previous);

	void on_treeWidget_repos_customContextMenuRequested(const QPoint &pos);
	void on_tableWidget_log_customContextMenuRequested(const QPoint &pos);
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

	void on_lineEdit_filter_textChanged(const QString &text);
	void on_toolButton_erase_filter_clicked();

	void on_tableWidget_log_itemDoubleClicked(QTableWidgetItem *);
	void on_listWidget_unstaged_itemDoubleClicked(QListWidgetItem *item);
	void on_listWidget_staged_itemDoubleClicked(QListWidgetItem *item);
	void on_listWidget_files_itemDoubleClicked(QListWidgetItem *item);

	void onRepositoriesTreeDropped();
	void on_action_set_config_user_triggered();

	void on_action_window_log_triggered(bool checked);

	void onLogVisibilityChanged();

	void on_action_repo_jump_triggered();
	void on_action_repo_checkout_triggered();
	void on_action_delete_branch_triggered();

	void on_action_push_u_origin_master_triggered();

	void on_toolButton_terminal_clicked();
	void on_toolButton_explorer_clicked();

	void on_action_push_u_triggered();

	void on_action_reset_HEAD_1_triggered();

	void on_action_create_a_repository_triggered();

	void onAvatarUpdated();
	void on_radioButton_remote_online_clicked();

	void on_radioButton_remote_offline_clicked();
	void on_verticalScrollBar_log_valueChanged(int);
	void on_horizontalScrollBar_log_valueChanged(int);

	void onPtyProcessCompleted();
	void on_toolButton_stop_process_clicked();

	void on_action_stop_process_triggered();

	void on_action_exit_triggered();

	void on_action_reflog_triggered();

signals:
	void onEscapeKeyPressed();
};

#endif // MAINWINDOW_H
