#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "Git.h"
#include "GitCommandRunner.h"
#include "MyProcess.h"
#include "RepositoryInfo.h"
#include "RepositoryModel.h"
#include "RepositoryTreeWidget.h"
#include "StatusInfo.h"
#include "TextEditorTheme.h"
#include "UserEvent.h"
#include <QMainWindow>
#include <memory>

class AddRepositoryDialog;
class ApplicationSettings;
class BranchLabel;
struct RepositoryInfo;
class GitObjectCache;
class QListWidget;
class QListWidgetItem;
class QTableWidgetItem;
class QTreeWidgetItem;
struct CommitLogExchangeData;
class ProgressWidget;

namespace Ui {
class MainWindow;
}

struct CloneParams {
	GitCloneData clonedata;
	RepositoryInfo repodata;
};
Q_DECLARE_METATYPE(CloneParams)

struct LogData {
	QByteArray data;
	LogData() = default;
	LogData(QByteArray const &ba)
		: data(ba)
	{
	}
	LogData(QString const &s)
		: data(s.toUtf8())
	{
	}
	LogData(char const *p, int n)
		: data(p, n)
	{
	}
	LogData(std::string_view const &s)
		: data(s.data(), s.size())
	{
	}
};
Q_DECLARE_METATYPE(LogData)

struct GitHubRepositoryInfo {
	QString owner_account_name;
	QString repository_name;
};

class HunkItem {
public:
	int hunk_number = -1;
	size_t pos, len;
	std::vector<std::string> lines;
};

class AbstractGitCommand;

class MainWindowExchangeData;

class PtyProcessCompleted {
public:
	std::function<void (ProcessStatus *, QVariant const &)> callback;
	std::shared_ptr<ProcessStatus> status;
	QVariant userdata;
	QString process_name;
	QElapsedTimer elapsed;
	PtyProcessCompleted()
		: status(std::make_shared<ProcessStatus>())
	{
	}
};
Q_DECLARE_METATYPE(PtyProcessCompleted)

class MainWindow : public QMainWindow {
	Q_OBJECT
	friend class UserEventHandler;
	friend class ImageViewWidget;
	friend class FileDiffSliderWidget;
	friend class FileHistoryWindow;
	friend class FileDiffWidget;
	friend class AboutDialog;
	friend class RepositoryTreeWidget; // TODO:
	friend class MainWindowExchangeData;
public:
	enum {
		IndexRole = Qt::UserRole,
		FilePathRole,
		DiffIndexRole,
		ObjectIdRole,
		HeaderRole,
		SubmodulePathRole,
		SubmoduleCommitIdRole,
	};
	enum CloneOperation {
		Clone,
		SubmoduleAdd,
	};

	enum InteractionMode {
		None,
		Busy,
	};

	enum NamedCommitFlag {
		Branches = 0x0001,
		Tags     = 0x0002,
		Remotes  = 0x0100,
	};

	enum class FileListType {
		MessagePanel,
		SingleList,
		SideBySide,
	};

	struct Task {
		int index = 0;
		int parent = 0;
		Task() = default;
		Task(int index, int parent)
			: index(index)
			, parent(parent)
		{
		}
	};

	struct Element {
		int depth = 0;
		std::vector<int> indexes;
	};

private:
	struct ObjectData {
		QString id;
		QString path;
		GitSubmoduleItem submod;
		GitCommitItem submod_commit;
		QString header;
		int idiff;
		bool staged = false;
	};
private:
	Ui::MainWindow *ui;

	struct Private;
	Private *m;

	struct RepositoryTreeIndex {
		int row = -1;
	};

	enum class FilterTarget {
		RepositorySearch,
		CommitLogSearch,
	};

	class GitFile {
	public:
		GitObject::Type type = GitObject::Type::NONE;
		QByteArray content;
		operator bool () const
		{
			return type != GitObject::Type::NONE;
		}
		bool is(GitObject::Type t) const
		{
			return t == type;
		}
	};

	void postEvent(QObject *receiver, QEvent *event, int ms_later);
	void postUserEvent(UserEventHandler::variant_t &&v, int ms_later);
	void cancelPendingUserEvents();

	void updateFileList(const GitHash &id);
	void updateFileList(const GitCommitItem *commit);
	void updateFileListLater(int delay_ms);
	void cancelUpdateFileList();
	void initUpdateFileListTimer();

	struct OpenRepositoryOption {
		bool new_session = true;

		bool validate = false;
		bool wait_cursor = true;
		bool keep_selection = false;

		bool clear_log = true;
		bool do_fetch = true;
	};
	void openRepositoryMain(OpenRepositoryOption const &opt);
	void openRepository(OpenRepositoryOption const &opt);
	void reopenRepository(bool validate);
	void openSelectedRepository();

	void doReopenRepository(ProcessStatus *status, const RepositoryInfo &repodata);

	QStringList selectedFiles_(QListWidget *listwidget) const;
	QStringList selectedFiles() const;
	void for_each_selected_files(std::function<void (QString const &)> const &fn);
	void clearFileList();
	void clearDiffView();

	RepositoryTreeIndex repositoryTreeIndex(const QTreeWidgetItem *item) const;
	std::optional<RepositoryInfo> repositoryItem(const RepositoryTreeIndex &index) const;

	void buildRepoTree(QString const &group, QTreeWidgetItem *item, QList<RepositoryInfo> *repos);
	void refrectRepositories();

	void updateDiffView(QListWidgetItem *item);
	void updateDiffView();
	void updateUnstagedFileCurrentItem();
	void updateStagedFileCurrentItem();
	void updateStatusBarText();
	void setRepositoryInfo(QString const &reponame, QString const &brname);

	QString getIncrementalSearchText() const;
	void setFilterText(const QString &text, int repo_list_select_row = -1);
	void clearFilterText(int repo_list_select_row = -1);
	void clearAllFilters(int select_row = -1);
	bool applyFilter();
	void _appendCharToFilterText(ushort c);
	bool appendCharToFilterText(int k, FilterTarget ft);
	MainWindow::FilterTarget filtertarget() const;

	void revertCommit();
	void mergeBranch(const QString &commit, GitMergeFastForward ff, bool squash);
	void mergeBranch(GitCommitItem const *commit, GitMergeFastForward ff, bool squash);
	void rebaseBranch(GitCommitItem const *commit);
	void cherrypick(GitCommitItem const *commit);
	void merge(GitCommitItem commit = {});
	void setRemoteOnline(bool f, bool save);
	void startTimers();
	void setNetworkingCommandsEnabled(bool enabled);
	void blame(QListWidgetItem *item);
	void blame();
	QListWidgetItem *currentFileItem() const;
	void execAreYouSureYouWantToContinueConnectingDialog(const QString &windowtitle);
	void deleteRemoteBranch(const GitCommitItem &commit);
	QStringList remoteBranches(const GitHash &id, QStringList *all);
	bool isUninitialized();
	void onLogCurrentItemChanged(bool update_file_list);
	void findNext();
	void findText(const QString &text);
	bool locateCommitID(QString const &commit_id);
	void showStatus();
	void onStartEvent();
	void showLogWindow(bool show);
	bool isValidRemoteURL(const QString &url, const QString &sshkey);
	QStringList whichCommand_(const QString &cmdfile1, const QString &cmdfile2 = {});
	QString selectCommand_(const QString &cmdname, const QStringList &cmdfiles, const QStringList &list, QString path, const std::function<void (const QString &)> &callback);
	QString selectCommand_(const QString &cmdname, const QString &cmdfile, const QStringList &list, const QString &path, const std::function<void (const QString &)> &callback);
	const RepositoryInfo *findRegisteredRepository(QString *workdir) const;
	bool execSetGlobalUserDialog();
	void revertAllFiles();
	bool execWelcomeWizardDialog();
	void execRepositoryPropertyDialog(const RepositoryInfo &repo, bool open_repository_menu = false);
	void execConfigUserDialog(const GitUser &global_user, const GitUser &local_user, bool enable_local_user, const QString &reponame);
	void setGitCommand(const QString &path, bool save);
	void setGpgCommand(const QString &path, bool save);
	void setSshCommand(const QString &path, bool save);
	bool checkGitCommand();
	bool saveBlobAs(const QString &id, const QString &dstpath);
	bool saveByteArrayAs(const QByteArray &ba, const QString &dstpath);
	bool saveFileAs(const QString &srcpath, const QString &dstpath);
	QString executableOrEmpty(const QString &path);
	bool checkExecutable(const QString &path);
	void internalSaveCommandPath(const QString &path, bool save, const QString &name);
	void logGitVersion();
	void internalClearRepositoryInfo();
	void checkUser();

	void setCurrentRepository(const RepositoryInfo &repo, bool clear_authentication);
	std::optional<QList<GitDiff>> makeDiffs(GitRunner g, GitHash id, std::future<QList<GitSubmoduleItem>> &&async_modules);

	void updateRemoteInfo();

	void submodule_add(QString url = {}, const QString &local_dir = {});
	const GitCommitItem &selectedCommitItem() const;
	void commit(bool amend = false);
	void commitAmend();
	
	void clone(CloneParams const &a);

	void push(bool set_upstream, const QString &remote, const QString &branch, bool force);
	void fetch(GitRunner g, bool prune);
	void stage(GitRunner g, const QStringList &paths);
	void fetch(GitRunner g);
	void pull(GitRunner g);
	void push_tags(GitRunner g);
	void delete_tags(GitRunner g, const QStringList &tagnames);
	void add_tag(GitRunner g, const QString &name, GitHash const &commit_id);

	bool push();

	void deleteBranch(const GitCommitItem &commit);
	void deleteSelectedBranch();
	void resetFile(const QStringList &paths);
	void clearAuthentication();
	void clearSshAuthentication();
	void internalDeleteTags(const QStringList &tagnames);
	void internalAddTag(const QString &name);
	void createRepository(const QString &dir);
	void addRepository(const QString &local_dir, const QString &group = {});
	void addRepositoryAccepted(const AddRepositoryDialog &dlg);
	void scanFolderAndRegister(const QString &group);
	void doGitCommand(const std::function<void (GitRunner)> &callback);
	void setWindowTitle_(const GitUser &user);
	void setUnknownRepositoryInfo();
	void setCurrentRemoteName(const QString &name);
	void deleteTags(const GitCommitItem &commit);
	QStringList remotes() const;
	BranchList findBranch(const GitHash &id);
	QString tempfileHeader() const;
	void deleteTempFiles();
	QString newTempFilePath();
	int limitLogCount() const;
	bool isThereUncommitedChanges() const;
	GitCommitItemList retrieveCommitLog(GitRunner g) const;
	const std::map<GitHash, BranchList> &branchmap() const;

	void updateWindowTitle(const GitUser &user);
	void updateWindowTitle(GitRunner g);

	std::tuple<QString, BranchLabelList> makeCommitLabels(GitCommitItem const &commit, std::map<GitHash, BranchList> const &branch_map, std::map<GitHash, TagList> const &tag_map) const;
	QString labelsInfoText(GitCommitItem const &commit);

	void removeRepositoryFromBookmark(RepositoryTreeIndex const &index, bool ask);
	void openTerminal(const RepositoryInfo *repo);
	void openExplorer(const RepositoryInfo *repo);
	bool askAreYouSureYouWantToRun(const QString &title, const QString &command);
	bool editFile(const QString &path, const QString &title);
	void setAppSettings(const ApplicationSettings &appsettings);
	QStringList findGitObject(const QString &id) const;

	void saveApplicationSettings();

	void setDiffResult(const QList<GitDiff> &diffs);
	const QList<GitSubmoduleItem> &submodules() const;
	void setSubmodules(const QList<GitSubmoduleItem> &submodules);
	bool runOnRepositoryDir(const std::function<void (QString, QString)> &callback, const RepositoryInfo *repo);
	NamedCommitList namedCommitItems(int flags);
	
	const std::map<GitHash, TagList> &tagmap() const;
	std::map<GitHash, TagList> queryTags(GitRunner g);
	TagList findTag(const GitHash &id) const;

	void sshSetPassphrase(const std::string &user, const std::string &pass);
	std::string sshPassphraseUser() const;
	std::string sshPassphrasePass() const;
	void httpSetAuthentication(const std::string &user, const std::string &pass);
	std::string httpAuthenticationUser() const;
	std::string httpAuthenticationPass() const;
	const GitCommitItem *getLog(int index) const;

	bool saveRepositoryBookmarks();
	QString getBookmarksFilePath() const;
	void stopPtyProcess();
	void abortPtyProcess();
	PtyProcess *getPtyProcess();
	PtyProcess const *getPtyProcess() const;
	bool getPtyProcessOk() const;
	bool isPtyProcessRunning() const;
	void setCompletedHandler(std::function<void (bool, const QVariant &)> fn, const QVariant &userdata);
	void setPtyProcessOk(bool pty_process_ok);

	const QList<RepositoryInfo> &repositoryList() const;
	void setRepositoryList(QList<RepositoryInfo> &&list);

	bool interactionEnabled() const;
	void setInteractionEnabled(bool enabled);
	InteractionMode interactionMode() const;
	void setInteractionMode(const InteractionMode &im);
	void setUncommitedChanges(bool uncommited_changes);
	QList<GitDiff> const *diffResult() const;

	void clearLabelMap();
	GitObjectCache *getObjCache();
	bool getForceFetch() const;
	void setForceFetch(bool force_fetch);
	GitHash getHeadId() const;
	void setHeadId(const GitHash &head_id);
	void setPtyProcessCompletionData(const QVariant &value);
	const QVariant &getTempRepoForCloneCompleteV() const;
	void msgNoRepositorySelected();
	bool isRepositoryOpened() const;
	void initRepository(const QString &path, const QString &reponame, const GitRemote &remote);
	void updatePocessLog(bool processevents);
	void appendLogHistory(const QByteArray &str);
	std::vector<std::string> getLogHistoryLines();
	void clearLogHistory();
	void updateAvatar(const GitUser &user, bool request);
	void cleanSubModule(GitRunner g, QListWidgetItem *item);

	void updateUncommitedChanges(GitRunner g);
	void enableDragAndDropOnRepositoryTree(bool enabled);
	QString preferredRepositoryGroup() const;
	void setPreferredRepositoryGroup(const QString &group);
	bool _addExistingLocalRepository(QString dir, QString name, QString sshkey, bool open, bool save = true, bool msgbox_if_err = true);
	void addExistingLocalRepositoryWithGroup(const QString &dir, const QString &group);
	bool addExistingLocalRepository(const QString &dir, bool open);
	QString currentFileMimeFileType() const;

	int rowFromCommitId(const GitHash &id);

	void _updateCommitLogTableView(int delay_ms);
	void makeCommitLog(GitHash const &head, CommitLogExchangeData exdata, int scroll_pos, int select_row);

	void updateButton();
	void runPtyGit(const QString &progress_message, GitRunner g, GitCommandRunner::variant_t var, std::function<void (ProcessStatus *, QVariant const &userdata)> callback, QVariant const &userdata);
	CommitLogExchangeData queryCommitLog(GitRunner g);
	void updateHEAD(GitRunner g);
	bool jump(GitRunner g, const GitHash &id);
	void jump(GitRunner g, const QString &text);
	void connectPtyProcessCompleted();
	void setupShowFileListHandler();
	
	void setRetry(std::function<void (const QVariant &)> fn, const QVariant &var);
	void clearRetry();
	void retry();
	bool isRetryQueued() const;
	void clearGitCommandCache();
	GitCommitItemList log_all2(GitRunner g, const GitHash &id, int maxcount) const;
	ProgressWidget *progress_widget() const;
	void internalShowPanel(FileListType file_list_type);
	
	void showFileList(FileListType files_list_type);
	void connectShowFileListHandler();
	void setupAddFileObjectData();
	void addFileObjectData(const MainWindowExchangeData &data);
	void setupStatusInfoHandler();
	void connectSetCommitLog();

	void _chooseRepository(QTreeWidgetItem *item);
	void chooseRepository();
	void setCurrentGitRunner(GitRunner g);
	void endSession();
protected:
	void closeEvent(QCloseEvent *event) override;
	void dragEnterEvent(QDragEnterEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;
	bool event(QEvent *event) override;
	bool eventFilter(QObject *watched, QEvent *event) override;
	std::optional<RepositoryInfo> selectedRepositoryItem() const;
	void removeSelectedRepositoryFromBookmark(bool ask);

protected slots:
	void onLogIdle();

private slots:
	void updateUI();
	void onLogVisibilityChanged();
	void onRepositoriesTreeDropped();
	void onInterval10ms();
	void onAvatarReady();
	void onCommitDetailGetterReady();
	void onPtyProcessCompleted(bool ok, PtyProcessCompleted const &data);
	void onSetCommitLog(const CommitLogExchangeData &log);
	void onCommitLogCurrentRowChanged(int row);

	void on_action_about_triggered();
	void on_action_add_repository_triggered();
	void on_action_clean_df_triggered();
	void on_action_commit_triggered();
	void on_action_create_desktop_launcher_file_triggered();
	void on_action_delete_branch_triggered();
	void on_action_delete_remote_branch_triggered();
	void on_action_edit_git_config_triggered();
	void on_action_edit_gitignore_triggered();
	void on_action_edit_global_gitconfig_triggered();
	void on_action_edit_settings_triggered();
	void on_action_edit_tags_triggered();
	void on_action_exit_triggered();
	void on_action_expand_commit_log_triggered();
	void on_action_expand_diff_view_triggered();
	void on_action_expand_file_list_triggered();
	void on_action_explorer_triggered();
	void on_action_fetch_prune_triggered();
	void on_action_fetch_triggered();
	void on_action_find_next_triggered();
	void on_action_find_triggered();
	void on_action_offline_triggered();
	void on_action_online_triggered();
	void on_action_pull_triggered();
	void on_action_push_all_tags_triggered();
	void on_action_push_triggered();
	void on_action_reflog_triggered();
	void on_action_repo_checkout_triggered();
	void on_action_repo_jump_to_head_triggered();
	void on_action_repo_jump_triggered();
	void on_action_repo_merge_triggered();
	void on_action_repositories_panel_triggered();
	void on_action_repository_property_triggered();
	void on_action_repository_status_triggered();
	void on_action_reset_HEAD_1_triggered();
	void on_action_reset_hard_triggered();
	void on_action_configure_user_triggered();
	void on_action_set_gpg_signing_triggered();
	void on_action_show_labels_triggered();
	void on_action_show_graph_triggered();
	void on_action_show_avatars_triggered();
	void on_action_sidebar_triggered();
	void on_action_stash_apply_triggered();
	void on_action_stash_drop_triggered();
	void on_action_stash_triggered();
	void on_action_stop_process_triggered();
	void on_action_submodule_add_triggered();
	void on_action_submodule_update_triggered();
	void on_action_submodules_triggered();
	void on_action_terminal_triggered();
	void on_action_view_refresh_triggered();
	void on_action_window_log_triggered(bool checked);
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
	void on_tableWidget_log_customContextMenuRequested(const QPoint &pos);
	void on_tableWidget_log_doubleClicked(const QModelIndex &index);
	void on_toolButton_commit_clicked();
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
	void on_toolButton_addrepo_clicked();

	void test();
	void toggleMaximized();

	void onRemoteInfoChanged();
	void onShowStatusInfo(StatusInfo const &info);
	void on_action_rebase_abort_triggered();

	void onShowFileList(FileListType panel_type);
	void onAddFileObjectData(const MainWindowExchangeData &data);

	void on_action_view_sort_by_time_changed();

	void on_action_ssh_triggered();

	void on_action_restart_trace_logger_triggered();

signals:
	void signalUpdateCommitLog();
	void signalSetProgress(float progress);
	void signalShowStatusInfo(StatusInfo const &info);
	void signalHideProgress();
	void sigWriteLog(LogData const &logdata);
	void sigShowFileList(FileListType files_list_type);
	void signalAddFileObjectData(const MainWindowExchangeData &data);
	void remoteInfoChanged();
	void sigPtyCloneCompleted(bool ok, QVariant const &userdata);
	void sigPtyFetchCompleted(bool ok, QVariant const &userdata);
	void sigPtyProcessCompleted(bool ok, PtyProcessCompleted const &data);
	void sigSetCommitLog(const CommitLogExchangeData &log);

public:
	explicit MainWindow(QWidget *parent = nullptr);
	~MainWindow() override;

	QColor color(unsigned int i);
	bool isOnlineMode() const;
	void updateCurrentFileList();
	RepositoryTreeWidget::RepositoryListStyle repositoriesListStyle() const;
	void updateRepositoryList(RepositoryTreeWidget::RepositoryListStyle style = RepositoryTreeWidget::RepositoryListStyle::_Keep, int select_row = -1, QString const &search_text = {});
	
	const TagList &queryCurrentCommitTagList() const;
	
	static int indexOfLog(QListWidgetItem *item);
	static int indexOfDiff(QListWidgetItem *item);
	static QList<GitSubmoduleItem> updateSubmodules(GitRunner g, const GitHash &id);
	static void updateCommitGraph(GitCommitItemList *logs);
	static TagList findTag(std::map<GitHash, TagList> const &tagmap, GitHash const &id);
	static QString makeRepositoryName(const QString &loc);
	static void addDiffItems(const QList<GitDiff> *diff_list, const std::function<void (const ObjectData &)> &add_item);
	static GitHash getObjectID(QListWidgetItem *item);
	static QString getFilePath(QListWidgetItem *item);
	static QString getSubmodulePath(QListWidgetItem *item);
	static QString getSubmoduleCommitId(QListWidgetItem *item);
	static std::pair<QString, QString> makeFileItemText(const ObjectData &data);
	static QListWidgetItem *newListWidgetFileItem(const MainWindow::ObjectData &data);
	static std::string parseDetectedDubiousOwnershipInRepositoryAt(const std::vector<std::string> &lines);
	// static QString treeItemName(QTreeWidgetItem *item);
	// static QString treeItemGroup(QTreeWidgetItem *item);
	static bool isValidWorkingCopy(GitRunner g);
	bool isValidWorkingCopy(QString const &local_dir);
	static QString abbrevCommitID(const GitCommitItem &commit);

	void drawDigit(QPainter *pr, int x, int y, int n) const;
	static constexpr int DIGIT_WIDTH = 5;
	static constexpr int DIGIT_HEIGHT = 7;
	void setStatusInfo(StatusInfo const &info);
	void clearStatusInfo();
	bool setCurrentLogRow(int row);
	bool shown();
	void deleteTags(QStringList const &tagnames);
	void addTag(QString const &name);
	int selectedLogIndex() const;
	void updateAncestorCommitMap();
	bool isAncestorCommit(GitHash const &id) const;
	void postStartEvent(int ms_later);
	void setShowLabels(bool show, bool save);
	void setShowGraph(bool show, bool save);
	void setShowAvatars(bool show, bool save);
	bool isLabelsVisible() const;
	bool isGraphVisible() const;
	bool isAvatarsVisible() const;
	bool isAvatarEnabled() const;
	void makeDiffList(const GitHash &id, QList<GitDiff> *diff_list, QListWidget *listwidget);
	void execCommitViewWindow(const GitCommitItem *commit);
	void execCommitPropertyDialog(QWidget *parent, const GitCommitItem &commit);
	void execCommitExploreWindow(QWidget *parent, const GitCommitItem *commit);
	void execFileHistory(const QString &path);
	void execFileHistory(QListWidgetItem *item);
	void showObjectProperty(QListWidgetItem *item);
	bool testRemoteRepositoryValidity(const QString &url, const QString &sshkey);
	QString selectGitCommand(bool save);
	QString selectGpgCommand(bool save);
	QString selectSshCommand(bool save);
	const GitBranch &currentBranch() const;
	void setCurrentBranch(const GitBranch &b);
	const RepositoryInfo &currentRepository() const;
	QString currentRepositoryName() const;
	QString currentRemoteName() const;
	QString currentBranchName() const;
	GitRunner _git(const QString &dir, const QString &submodpath, const QString &sshkey) const;
	GitRunner unassosiated_git_runner() const;
	GitRunner new_git_runner(const QString &dir, const QString &sshkey);
	GitRunner new_git_runner();
	GitRunner git();
	static GitRunner git_for_submodule(GitRunner g, GitSubmoduleItem const &submod);
	bool isValidWorkingCopy(QString const &dir) const;
	void autoOpenRepository(QString dir, const QString &commit_id = {});
	std::optional<GitCommitItem> queryCommit(const GitHash &id);
	bool checkoutLocalBranch(QString const &name);
	void checkout(QWidget *parent, const GitCommitItem &commit, std::function<void ()> accepted_callback = {});
	void checkout();
	bool jumpToCommit(const GitHash &id);
	bool jumpToCommit(const QString &id);

	GitObject catFile(GitRunner g, const QString &id);
	bool saveAs(const QString &id, const QString &dstpath);

	TextEditorThemePtr themeForTextEditor();
	void emitWriteLog(LogData const &logdata);
	QString findFileID(const GitHash &commit_id, const QString &file);
	const GitCommitItem &commitItem(int row) const;
	const GitCommitItem &commitItem(GitHash const &id) const;
	QImage committerIcon(int row, QSize size) const;
	void changeSshKey(const QString &local_dir, const QString &ssh_key, bool save);
	ApplicationSettings *appsettings();
	const ApplicationSettings *appsettings() const;
	QString defaultWorkingDir() const;
	QIcon signatureVerificationIcon(const GitHash &id) const;
	QAction *addMenuActionProperty(QMenu *menu);
	QString currentWorkingCopyDir() const;

	void refresh();
	bool cloneRepository(const GitCloneData &clonedata, const RepositoryInfo &repodata);
	GitUser currentGitUser() const;
	void setupExternalPrograms();
	void updateCommitLogTableViewLater();
	void saveRepositoryBookmark(RepositoryInfo item);
	void changeRepositoryBookmarkName(RepositoryInfo item, QString new_name);
	BranchLabelList rowLabels(int row, bool sorted = true) const;
	void setProgress(float progress);
	void showProgress(const QString &text, float progress = -1.0f);
	void hideProgress();
	void internalAfterFetch();
	void onRepositoryTreeSortRecent(bool f);
	const GitCommitItemList &commitlog() const;
	const GitCommitItem *currentCommitItem();
	void clearLogContents();
	void updateLogTableView();
	void setFocusToLogTable();
	void selectLogTableRow(int row);
	RepositoryData *currentRepositoryData();
	const RepositoryData *currentRepositoryData() const;
	void setCommitLog(const CommitLogExchangeData &exdata);
public slots:
	void internalWriteLog(const LogData &logdata);
};

class MainWindowExchangeData {
public:
	MainWindow::FileListType files_list_type;
	std::vector<MainWindow::ObjectData> object_data;
};
Q_DECLARE_METATYPE(MainWindowExchangeData)

#endif // MAINWINDOW_H
