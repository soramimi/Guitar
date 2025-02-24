#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "RepositoryModel.h"
#include "Git.h"
#include "GitCommandRunner.h"
#include "MyProcess.h"
#include "RepositoryInfo.h"
#include "RepositoryTreeWidget.h"
#include "TextEditorTheme.h"
#include "UserEvent.h"
#include "StatusInfo.h"
#include <QMainWindow>

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

namespace Ui {
class MainWindow;
}

struct CloneParams {
	Git::CloneData clonedata;
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
	std::function<void (ProcessStatus const &, QVariant const &)> callback;
	ProcessStatus status;
	QVariant userdata;
	QString process_name;
	QElapsedTimer elapsed;
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

	enum {
		GroupItem = -1,
	};

private:
	struct ObjectData {
		QString id;
		QString path;
		Git::SubmoduleItem submod;
		Git::CommitItem submod_commit;
		QString header;
		int idiff;
		bool staged = false;
	};
private:

	Ui::MainWindow *ui;

	struct Private;
	Private *m;

public:
	explicit MainWindow(QWidget *parent = nullptr);
	~MainWindow() override;

	QColor color(unsigned int i);

	bool isOnlineMode() const;
private:
	struct OpenRepositoyOption {
		bool validate = false;
		bool waitcursor = true;
		bool keep_selection = false;
	};

	void postEvent(QObject *receiver, QEvent *event, int ms_later);
	void postUserEvent(UserEventHandler::variant_t &&v, int ms_later);

	void updateFileList(const Git::Hash &id);
	void updateFileList(const Git::CommitItem *commit);
	void updateFileListLater(int delay_ms);
	void cancelUpdateFileList();
public:
	void updateCurrentFileList();
public:
	RepositoryTreeWidget::RepositoryListStyle repositoriesListStyle() const;
	void updateRepositoryList(RepositoryTreeWidget::RepositoryListStyle style = RepositoryTreeWidget::RepositoryListStyle::_Keep, int select_row = -1);
private:
	void openRepositoryMain(GitRunner g, bool clear_log, bool do_fetch, bool keep_selection);

	QStringList selectedFiles_(QListWidget *listwidget) const;
	QStringList selectedFiles() const;
	void for_each_selected_files(std::function<void (QString const &)> const &fn);
	void clearFileList();
	void clearDiffView();

	struct RepositoryTreeIndex {
		int row = -1;
	};
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

	enum class FilterTarget {
		RepositorySearch,
		CommitLogSearch,
	};
	QString getFilterText(FilterTarget ft) const;
	void setFilterText(FilterTarget ft, const QString &text, int select_row = -1);
	void clearFilterText(FilterTarget ft, int select_row = -1);
	void clearAllFilters();
	void appendCharToFilterText(FilterTarget ft, ushort c);

	void revertCommit();
	void mergeBranch(const QString &commit, Git::MergeFastForward ff, bool squash);
	void mergeBranch(Git::CommitItem const *commit, Git::MergeFastForward ff, bool squash);
	void rebaseBranch(Git::CommitItem const *commit);
	void cherrypick(Git::CommitItem const *commit);
	void merge(Git::CommitItem commit = {});
	void setRemoteOnline(bool f, bool save);
	void startTimers();
	void setNetworkingCommandsEnabled(bool enabled);
	void blame(QListWidgetItem *item);
	void blame();
	QListWidgetItem *currentFileItem() const;
	void execAreYouSureYouWantToContinueConnectingDialog(const QString &windowtitle);
	void deleteRemoteBranch(const Git::CommitItem &commit);
	QStringList remoteBranches(const Git::Hash &id, QStringList *all);
	bool isUninitialized();
	void onLogCurrentItemChanged();
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
	void execConfigUserDialog(const Git::User &global_user, const Git::User &local_user, bool enable_local_user, const QString &reponame);
	void setGitCommand(const QString &path, bool save);
	void setGpgCommand(const QString &path, bool save);
	void setSshCommand(const QString &path, bool save);
	bool checkGitCommand();
	bool saveBlobAs(const QString &id, const QString &dstpath);
	bool saveByteArrayAs(const QByteArray &ba, const QString &dstpath);
	static QString makeRepositoryName(const QString &loc);
	bool saveFileAs(const QString &srcpath, const QString &dstpath);
	QString executableOrEmpty(const QString &path);
	bool checkExecutable(const QString &path);
	void internalSaveCommandPath(const QString &path, bool save, const QString &name);
	void logGitVersion();
	void internalClearRepositoryInfo();
	void checkUser();

	void openRepository(OpenRepositoyOption const &opt);
	void reopenRepository(bool validate);

	void setCurrentRepository(const RepositoryInfo &repo, bool clear_authentication);
	void openSelectedRepository();
	std::optional<QList<Git::Diff> > makeDiffs(GitRunner g, Git::Hash id);
	void updateRemoteInfo(GitRunner g);
	void queryRemotes(GitRunner g);
	void submodule_add(QString url = {}, const QString &local_dir = {});
	const Git::CommitItem &selectedCommitItem() const;
	void commit(bool amend = false);
	void commitAmend();
	
	void clone(CloneParams const &a);

	void push(bool set_upstream, const QString &remote, const QString &branch, bool force);
	void fetch(GitRunner g, bool prune);
	void stage(GitRunner g, const QStringList &paths);
	void fetch_tags_f(GitRunner g);
	void pull(GitRunner g);
	void push_tags(GitRunner g);
	void delete_tags(GitRunner g, const QStringList &tagnames);
	void add_tag(GitRunner g, const QString &name, Git::Hash const &commit_id);

	bool push();

	void deleteBranch(const Git::CommitItem &commit);
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
	void setWindowTitle_(const Git::User &user);
	void setUnknownRepositoryInfo();
	void setCurrentRemoteName(const QString &name);
	void deleteTags(const Git::CommitItem &commit);
	bool isAvatarEnabled() const;
	QStringList remotes() const;
	BranchList findBranch(const Git::Hash &id);
	QString tempfileHeader() const;
	void deleteTempFiles();
	QString newTempFilePath();
	int limitLogCount() const;
	Git::Object internalCatFile(GitRunner g, const QString &id);
	bool isThereUncommitedChanges() const;
	static void addDiffItems(const QList<Git::Diff> *diff_list, const std::function<void (const ObjectData &)> &add_item);
	Git::CommitItemList retrieveCommitLog(GitRunner g) const;
	const std::map<Git::Hash, BranchList> &branchmap() const;

	void updateWindowTitle(const Git::User &user);
	void updateWindowTitle(GitRunner g);

	std::tuple<QString, BranchLabelList> makeCommitLabels(Git::CommitItem const &commit, std::map<Git::Hash, BranchList> const &branch_map, std::map<Git::Hash, TagList> const &tag_map) const;
	QString labelsInfoText(Git::CommitItem const &commit);

	void removeRepositoryFromBookmark(RepositoryTreeIndex const &index, bool ask);
	void openTerminal(const RepositoryInfo *repo);
	void openExplorer(const RepositoryInfo *repo);
	bool askAreYouSureYouWantToRun(const QString &title, const QString &command);
	bool editFile(const QString &path, const QString &title);
	void setAppSettings(const ApplicationSettings &appsettings);
	QStringList findGitObject(const QString &id) const;
	void saveApplicationSettings();
	void loadApplicationSettings();
	void setDiffResult(const QList<Git::Diff> &diffs);
	const QList<Git::SubmoduleItem> &submodules() const;
	void setSubmodules(const QList<Git::SubmoduleItem> &submodules);
	bool runOnRepositoryDir(const std::function<void (QString, QString)> &callback, const RepositoryInfo *repo);
	NamedCommitList namedCommitItems(int flags);
	static Git::Hash getObjectID(QListWidgetItem *item);
	static QString getFilePath(QListWidgetItem *item);
	static QString getSubmodulePath(QListWidgetItem *item);
	static QString getSubmoduleCommitId(QListWidgetItem *item);
	static bool isGroupItem(QTreeWidgetItem *item);
	static int indexOfLog(QListWidgetItem *item);
	static int indexOfDiff(QListWidgetItem *item);
	static void updateSubmodules(GitRunner g, const Git::Hash &id, QList<Git::SubmoduleItem> *out);
	void saveRepositoryBookmark(RepositoryInfo item);
	void changeRepositoryBookmarkName(RepositoryInfo item, QString new_name);

public:
	const TagList &queryCurrentCommitTagList() const;
private:
	const std::map<Git::Hash, TagList> &tagmap() const;
	std::map<Git::Hash, TagList> queryTags(GitRunner g);
	static TagList findTag(std::map<Git::Hash, TagList> const &tagmap, Git::Hash const &id);
	TagList findTag(const Git::Hash &id) const;

	void sshSetPassphrase(const std::string &user, const std::string &pass);
	std::string sshPassphraseUser() const;
	std::string sshPassphrasePass() const;
	void httpSetAuthentication(const std::string &user, const std::string &pass);
	std::string httpAuthenticationUser() const;
	std::string httpAuthenticationPass() const;
	const Git::CommitItem *getLog(int index) const;

	static void updateCommitGraph(Git::CommitItemList *logs);

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
	QList<Git::Diff> const *diffResult() const;

	void clearLabelMap();
	GitObjectCache *getObjCache();
	bool getForceFetch() const;
	void setForceFetch(bool force_fetch);
	Git::Hash getHeadId() const;
	void setHeadId(const Git::Hash &head_id);
	void setPtyProcessCompletionData(const QVariant &value);
	const QVariant &getTempRepoForCloneCompleteV() const;
	void msgNoRepositorySelected();
	bool isRepositoryOpened() const;
	static std::pair<QString, QString> makeFileItemText(const ObjectData &data);
	static QListWidgetItem *newListWidgetFileItem(const MainWindow::ObjectData &data);
	void cancelPendingUserEvents();
	void initRepository(const QString &path, const QString &reponame, const Git::Remote &remote);
	void updatePocessLog(bool processevents);
	void appendLogHistory(const QByteArray &str);
	std::vector<std::string> getLogHistoryLines();
	void clearLogHistory();
	void updateAvatar(const Git::User &user, bool request);
	void cleanSubModule(QListWidgetItem *item);

	class GitFile {
	public:
		Git::Object::Type type = Git::Object::Type::NONE;
		QByteArray content;
		operator bool () const
		{
			return type != Git::Object::Type::NONE;
		}
		bool is(Git::Object::Type t) const
		{
			return t == type;
		}
	};

	void updateUncommitedChanges();
	void enableDragAndDropOnRepositoryTree(bool enabled);
	static QString treeItemName(QTreeWidgetItem *item);
	static QString treeItemGroup(QTreeWidgetItem *item);
	QString preferredRepositoryGroup() const;
	void setPreferredRepositoryGroup(const QString &group);
	bool addExistingLocalRepository(QString dir, QString name, QString sshkey, bool open, bool save = true, bool msgbox_if_err = true);
	void addExistingLocalRepositoryWithGroup(const QString &dir, const QString &group);
	bool addExistingLocalRepository(const QString &dir, bool open);
	QString currentFileMimeFileType() const;
protected:
	void closeEvent(QCloseEvent *event) override;
	void dragEnterEvent(QDragEnterEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;
	bool event(QEvent *event) override;
	bool eventFilter(QObject *watched, QEvent *event) override;
	std::optional<RepositoryInfo> selectedRepositoryItem() const;
	void removeSelectedRepositoryFromBookmark(bool ask);
public:
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
	bool isAncestorCommit(const QString &id);
	void postStartEvent(int ms_later);
	void setShowLabels(bool show, bool save);
	void setShowGraph(bool show, bool save);
	void setShowAvatars(bool show, bool save);
	bool isLabelsVisible() const;
	bool isGraphVisible() const;
	bool isAvatarsVisible() const;
	void makeDiffList(const Git::Hash &id, QList<Git::Diff> *diff_list, QListWidget *listwidget);
	void execCommitViewWindow(const Git::CommitItem *commit);
	void execCommitPropertyDialog(QWidget *parent, const Git::CommitItem &commit);
	void execCommitExploreWindow(QWidget *parent, const Git::CommitItem *commit);
	void execFileHistory(const QString &path);
	void execFileHistory(QListWidgetItem *item);
	void showObjectProperty(QListWidgetItem *item);
	bool testRemoteRepositoryValidity(const QString &url, const QString &sshkey);
	QString selectGitCommand(bool save);
	QString selectGpgCommand(bool save);
	QString selectSshCommand(bool save);
	const Git::Branch &currentBranch() const;
	void setCurrentBranch(const Git::Branch &b);
	const RepositoryInfo &currentRepository() const;
	QString currentRepositoryName() const;
	QString currentRemoteName() const;
	QString currentBranchName() const;
	GitRunner git(const QString &dir, const QString &submodpath, const QString &sshkey) const;
	GitRunner git();
	GitRunner git(Git::SubmoduleItem const &submod);
	void autoOpenRepository(QString dir, const QString &commit_id = {});
	std::optional<Git::CommitItem> queryCommit(const Git::Hash &id);
	bool checkoutLocalBranch(QString const &name);
	void checkout(QWidget *parent, const Git::CommitItem &commit, std::function<void ()> accepted_callback = {});
	void checkout();
private:
	int rowFromCommitId(const Git::Hash &id);
public:
	bool jumpToCommit(const Git::Hash &id);
	bool jumpToCommit(const QString &id);

	Git::Object internalCatFile(const QString &id);
	Git::Object catFile(const QString &id);
	bool saveAs(const QString &id, const QString &dstpath);
	QString determinFileType(QByteArray const &in) const;
	QString determinFileType(const QString &path) const;

	TextEditorThemePtr themeForTextEditor();
	static bool isValidWorkingCopy(GitRunner g);
	void emitWriteLog(LogData const &logdata);
	QString findFileID(const QString &commit_id, const QString &file);
	const Git::CommitItem &commitItem(int row) const;
	const Git::CommitItem &commitItem(Git::Hash const &id) const;
	QImage committerIcon(int row, QSize size) const;
	void changeSshKey(const QString &local_dir, const QString &ssh_key, bool save);
	static QString abbrevCommitID(const Git::CommitItem &commit);
	ApplicationSettings *appsettings();
	const ApplicationSettings *appsettings() const;
	QString defaultWorkingDir() const;
	QIcon signatureVerificationIcon(const Git::Hash &id) const;
	QAction *addMenuActionProperty(QMenu *menu);
	QString currentWorkingCopyDir() const;
	Git::SubmoduleItem const *querySubmoduleByPath(const QString &path, Git::CommitItem *commit);
	void refresh();
	bool cloneRepository(const Git::CloneData &clonedata, const RepositoryInfo &repodata);
	Git::User currentGitUser() const;
	void setupExternalPrograms();
private:
	void _updateCommitLogTableView(int delay_ms);
public:
	void updateCommitLogTableViewLater();

	BranchLabelList rowLabels(int row, bool sorted = true) const;
private:
	void makeCommitLog(CommitLogExchangeData exdata, int scroll_pos, int select_row);
signals:
	void signalUpdateCommitLog();

public slots:
	void internalWriteLog(const LogData &logdata);
private slots:
	void updateUI();
	void onLogVisibilityChanged();
	void onRepositoriesTreeDropped();
	void onInterval10ms();
	void onAvatarReady();
	void onCommitDetailGetterReady();

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
	void on_tableWidget_log_customContextMenuRequested(const QPoint &pos);
	void on_tableWidget_log_doubleClicked(const QModelIndex &index);
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
	void on_toolButton_addrepo_clicked();

	void test();
	void toggleMaximized();

	void onRemoteInfoChanged();
	void onShowStatusInfo(StatusInfo const &info);
	void on_action_rebase_abort_triggered();

	void onShowFileList(FileListType files_list_type);
	void onAddFileObjectData(const MainWindowExchangeData &data);
private:
	void setupStatusInfoHandler();
public:
	void setProgress(float progress);
	void showProgress(const QString &text, float progress = -1.0f);
	void hideProgress();
signals:
	void signalSetProgress(float progress);
	void signalShowStatusInfo(StatusInfo const &info);
	void signalHideProgress();
protected slots:
	void onLogIdle();
signals:
	void sigWriteLog(LogData const &logdata);
private:
	void showFileList(FileListType files_list_type);
signals:
	void sigShowFileList(FileListType files_list_type);
private:
	void connectShowFileListHandler();

private:
	void setupAddFileObjectData();
	void addFileObjectData(const MainWindowExchangeData &data);

signals:
	void signalAddFileObjectData(const MainWindowExchangeData &data);
	void remoteInfoChanged();
private:

	void updateButton();
	void runPtyGit(const QString &progress_message, GitRunner g, GitCommandRunner::variant_t var, std::function<void (const ProcessStatus &, QVariant const &userdata)> callback, QVariant const &userdata);
	CommitLogExchangeData queryCommitLog(GitRunner g);
	void updateHEAD(GitRunner g);
	bool jump(GitRunner g, const Git::Hash &id);
	void jump(GitRunner g, const QString &text);
	void connectPtyProcessCompleted();
	void setupShowFileListHandler();
	
	void doReopenRepository(const ProcessStatus &status, const RepositoryInfo &repodata);
	void setRetry(std::function<void (const QVariant &)> fn, const QVariant &var);
	void clearRetry();
	void retry();
	bool isRetryQueued() const;
	static std::string parseDetectedDubiousOwnershipInRepositoryAt(const std::vector<std::string> &lines);
	void initUpdateFileListTimer();
	void clearGitCommandCache();
	Git::CommitItemList log_all2(GitRunner g, const Git::Hash &id, int maxcount) const;
private slots:
	void onPtyProcessCompleted(bool ok, PtyProcessCompleted const &data);

	void onSetCommitLog(const CommitLogExchangeData &log);
	void connectSetCommitLog();

	void onCommitLogCurrentRowChanged(int row);
public:
	void setCommitLog(const CommitLogExchangeData &exdata);

signals:
	void sigPtyCloneCompleted(bool ok, QVariant const &userdata);
	void sigPtyFetchCompleted(bool ok, QVariant const &userdata);
	void sigPtyProcessCompleted(bool ok, PtyProcessCompleted const &data);
	void sigSetCommitLog(const CommitLogExchangeData &log);
public:
	void internalAfterFetch();
	
	void onRepositoryTreeSortRecent();

public:
	const Git::CommitItemList &commitlog() const;
	const Git::CommitItem *currentCommitItem();

	void clearLogContents();
	void updateLogTableView();
	void setFocusToLogTable();
	void selectLogTableRow(int row);

	RepositoryData *currentRepositoryData();
	const RepositoryData *currentRepositoryData() const;
};

class MainWindowExchangeData {
public:
	MainWindow::FileListType files_list_type;
	std::vector<MainWindow::ObjectData> object_data;
};
Q_DECLARE_METATYPE(MainWindowExchangeData)

#endif // MAINWINDOW_H
