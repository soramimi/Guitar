#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "BranchLabel.h"
#include "Git.h"
#include "MyProcess.h"
#include "RepositoryData.h"
#include "RepositoryWrapperFrame.h"
#include "TextEditorTheme.h"
#include <QMainWindow>
#include <QThread>

class ApplicationSettings;
class AvatarLoader;
class GitObjectCache;
class QListWidget;
class QListWidgetItem;
class QTableWidgetItem;
class QTreeWidgetItem;
class RepositoryWrapperFrame;
class WebContext;
class GitProcessRequest;
struct GitHubRepositoryInfo;

namespace Ui {
class MainWindow;
}

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

class AbstractGitCommandItem;

class ExchangeData;

class PtyProcessCompleted {
public:
	ProcessStatus status;
	std::function<void (ProcessStatus const &status, QVariant const &)> callback;
	QVariant userdata;
};
Q_DECLARE_METATYPE(PtyProcessCompleted)

class MainWindow : public QMainWindow {
	Q_OBJECT
	friend class MainWindowHelperThread;
	friend class RepositoryWrapperFrame;
	friend class SubmoduleMainWindow;
	friend class ImageViewWidget;
	friend class FileDiffSliderWidget;
	friend class FileHistoryWindow;
	friend class FileDiffWidget;
	friend class AboutDialog;
	friend class RepositoriesTreeWidget; // TODO:
	friend class ExchangeData;
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

	enum class FilesListType {
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

	RepositoryWrapperFrame *frame();
	RepositoryWrapperFrame const *frame() const;

	QPixmap const &digitsPixmap() const;

	QColor color(unsigned int i);

	bool isOnlineMode() const;
private:
	struct BasicCommitLog {
		Git::CommitItemList commit_log;
		std::map<Git::CommitID, QList<Git::Branch>> branch_map;
	};


	void postEvent(QObject *receiver, QEvent *event, int ms_later);
	void postUserFunctionEvent(const std::function<void (const QVariant &, void *)> fn, QVariant const &v = QVariant(), void *p = nullptr, int ms_later = 0);

	void updateFileList(RepositoryWrapperFrame *frame, const Git::CommitID &id);
	void updateFileList(RepositoryWrapperFrame *frame, Git::CommitItem const &commit);
	void updateRepositoriesList();

	void internalOpenRepository(GitPtr g, bool fetch, bool keep_selection);

	void openRepositoryMain(RepositoryWrapperFrame *frame, GitPtr g, bool query, bool clear_log, bool do_fetch, bool keep_selection);

	QStringList selectedFiles_(QListWidget *listwidget) const;
	QStringList selectedFiles() const;
	void for_each_selected_files(std::function<void (QString const &)> const &fn);
	static void clearFileList(RepositoryWrapperFrame *frame);
    static void clearDiffView(RepositoryWrapperFrame *frame);
	void clearDiffView();

	int repositoryIndex_(const QTreeWidgetItem *item) const;
	RepositoryData const *repositoryItem(const QTreeWidgetItem *item) const;

	QTreeWidgetItem *newQTreeWidgetFolderItem(QString const &name);
	void buildRepoTree(QString const &group, QTreeWidgetItem *item, QList<RepositoryData> *repos);
	void refrectRepositories();

	void updateDiffView(RepositoryWrapperFrame *frame, QListWidgetItem *item);
	void updateDiffView(RepositoryWrapperFrame *frame);
	void updateUnstagedFileCurrentItem(RepositoryWrapperFrame *frame);
	void updateStagedFileCurrentItem(RepositoryWrapperFrame *frame);
	void updateStatusBarText(RepositoryWrapperFrame *frame);
	void setRepositoryInfo(QString const &reponame, QString const &brname);
	int indexOfRepository(const QTreeWidgetItem *treeitem) const;
	void clearRepoFilter();
	void appendCharToRepoFilter(ushort c);
	void backspaceRepoFilter();
	void revertCommit(RepositoryWrapperFrame *frame);
	void mergeBranch(const QString &commit, Git::MergeFastForward ff, bool squash);
	void mergeBranch(Git::CommitItem const *commit, Git::MergeFastForward ff, bool squash);
	void rebaseBranch(Git::CommitItem const *commit);
	void cherrypick(Git::CommitItem const *commit);
	void merge(RepositoryWrapperFrame *frame, Git::CommitItem commit = {});
	void detectGitServerType(GitPtr g);
	void setRemoteOnline(bool f, bool save);
	void startTimers();
	void setNetworkingCommandsEnabled(bool enabled);
	void blame(QListWidgetItem *item);
	void blame();
	QListWidgetItem *currentFileItem() const;
	void execAreYouSureYouWantToContinueConnectingDialog();
	void deleteRemoteBranch(RepositoryWrapperFrame *frame, const Git::CommitItem &commit);
	QStringList remoteBranches(RepositoryWrapperFrame *frame, const Git::CommitID &id, QStringList *all);
	bool isUninitialized();
	void onLogCurrentItemChanged(RepositoryWrapperFrame *frame);
	void findNext(RepositoryWrapperFrame *frame);
	void findText(const QString &text);
	bool locateCommitID(RepositoryWrapperFrame *frame, QString const &commit_id);
	void showStatus();
	void onStartEvent();
	void showLogWindow(bool show);
	bool isValidRemoteURL(const QString &url, const QString &sshkey);
	static QStringList whichCommand_(const QString &cmdfile1, const QString &cmdfile2 = {});
	QString selectCommand_(const QString &cmdname, const QStringList &cmdfiles, const QStringList &list, QString path, const std::function<void (const QString &)> &callback);
	QString selectCommand_(const QString &cmdname, const QString &cmdfile, const QStringList &list, const QString &path, const std::function<void (const QString &)> &callback);
	const RepositoryData *findRegisteredRepository(QString *workdir) const;
	static bool git_log_callback(void *cookie, const char *ptr, int len);
	bool execSetGlobalUserDialog();
	void revertAllFiles();
	bool execWelcomeWizardDialog();
	void execRepositoryPropertyDialog(const RepositoryData &repo, bool open_repository_menu = false);
	void execConfigUserDialog(const Git::User &global_user, const Git::User &local_user, bool enable_local_user, const QString &reponame);
	void setGitCommand(const QString &path, bool save);
	void setGpgCommand(const QString &path, bool save);
	void setSshCommand(const QString &path, bool save);
	bool checkGitCommand();
	bool saveBlobAs(RepositoryWrapperFrame *frame, const QString &id, const QString &dstpath);
	bool saveByteArrayAs(const QByteArray &ba, const QString &dstpath);
	static QString makeRepositoryName(const QString &loc);
	bool saveFileAs(const QString &srcpath, const QString &dstpath);
	QString executableOrEmpty(const QString &path);
	bool checkExecutable(const QString &path);
	static void internalSaveCommandPath(const QString &path, bool save, const QString &name);
	void logGitVersion();
	void internalClearRepositoryInfo();
	void checkUser();

	void openRepository(bool validate, bool waitcursor, bool keep_selection);
	void reopenRepository();

	void setCurrentRepository(const RepositoryData &repo, bool clear_authentication);
	void openSelectedRepository();
	std::optional<QList<Git::Diff> > makeDiffs(GitPtr g, RepositoryWrapperFrame *frame, Git::CommitID id);
	void updateRemoteInfo(GitPtr g);
	void queryRemotes(GitPtr g);
	void submodule_add(QString url = {}, const QString &local_dir = {});
	Git::CommitItem selectedCommitItem(RepositoryWrapperFrame *frame) const;
	void commit(RepositoryWrapperFrame *frame, bool amend = false);
	void commitAmend(RepositoryWrapperFrame *frame);
	
	void clone(GitPtr g, const Git::CloneData &clonedata, const RepositoryData &repodata);
	void push(bool set_upstream, const QString &remote, const QString &branch, bool force);
	void fetch(GitPtr g, bool prune);
	void stage(GitPtr g, const QStringList &paths);
	void fetch_tags_f(GitPtr g);
	void pull(GitPtr g);
	void push_tags(GitPtr g);
	void delete_tags(GitPtr g, const QStringList &tagnames);
	void add_tag(GitPtr g, const QString &name, Git::CommitID const &commit_id);

	bool push();

	void deleteBranch(RepositoryWrapperFrame *frame, const Git::CommitItem &commit);
	void deleteSelectedBranch(RepositoryWrapperFrame *frame);
	void resetFile(const QStringList &paths);
	void clearAuthentication();
	void clearSshAuthentication();
	void internalDeleteTags(const QStringList &tagnames);
	void internalAddTag(RepositoryWrapperFrame *frame, const QString &name);
	void createRepository(const QString &dir);
	void addRepository(const QString &local_dir, const QString &group = {});
	void scanFolderAndRegister(const QString &group);
	void setLogEnabled(GitPtr g, bool f);
	void doGitCommand(const std::function<void (GitPtr)> &callback);
	void setWindowTitle_(const Git::User &user);
	void setUnknownRepositoryInfo();
	void setCurrentRemoteName(const QString &name);
	void deleteTags(RepositoryWrapperFrame *frame, const Git::CommitItem &commit);
	bool isAvatarEnabled() const;
	QStringList remotes() const;
	static QList<Git::Branch> findBranch(RepositoryWrapperFrame *frame, const Git::CommitID &id);
	QString tempfileHeader() const;
	void deleteTempFiles();
	Git::CommitID idFromTag(RepositoryWrapperFrame *frame, const QString &tag);
	QString newTempFilePath();
	int limitLogCount() const;
	Git::Object internalCatFile(RepositoryWrapperFrame *frame, GitPtr g, const QString &id);
	bool isThereUncommitedChanges() const;
	static void addDiffItems(const QList<Git::Diff> *diff_list, const std::function<void (const ObjectData &)> &add_item);
	Git::CommitItemList retrieveCommitLog(GitPtr g) const;
	static std::map<Git::CommitID, QList<Git::Branch> > &commitToBranchMapRef(RepositoryWrapperFrame *frame);

	void updateWindowTitle(const Git::User &user);
	void updateWindowTitle(GitPtr g);

	QString makeCommitInfoText(RepositoryWrapperFrame *frame, int row, QList<BranchLabel> *label_list, bool lock);
	void removeRepositoryFromBookmark(int index, bool ask);
	void openTerminal(const RepositoryData *repo);
	void openExplorer(const RepositoryData *repo);
	bool askAreYouSureYouWantToRun(const QString &title, const QString &command);
	bool editFile(const QString &path, const QString &title);
	void setAppSettings(const ApplicationSettings &appsettings);
	QIcon getRepositoryIcon() const;
	QIcon getFolderIcon() const;
	QIcon getSignatureGoodIcon() const;
	QIcon getSignatureDubiousIcon() const;
	QIcon getSignatureBadIcon() const;
	QPixmap getTransparentPixmap() const;
	QStringList findGitObject(const QString &id) const;
	QList<BranchLabel> sortedLabels(RepositoryWrapperFrame *frame, int row) const;
	void saveApplicationSettings();
	void loadApplicationSettings();
	void setDiffResult(const QList<Git::Diff> &diffs);
	const QList<Git::SubmoduleItem> &submodules() const;
	void setSubmodules(const QList<Git::SubmoduleItem> &submodules);
	bool runOnRepositoryDir(const std::function<void (QString, QString)> &callback, const RepositoryData *repo);
	NamedCommitList namedCommitItems(RepositoryWrapperFrame *frame, int flags);
	static Git::CommitID getObjectID(QListWidgetItem *item);
	static QString getFilePath(QListWidgetItem *item);
	static QString getSubmodulePath(QListWidgetItem *item);
	static QString getSubmoduleCommitId(QListWidgetItem *item);
	static bool isGroupItem(QTreeWidgetItem *item);
	static int indexOfLog(QListWidgetItem *item);
	static int indexOfDiff(QListWidgetItem *item);
	static void updateSubmodules(GitPtr g, const Git::CommitID &id, QList<Git::SubmoduleItem> *out);
	void saveRepositoryBookmark(RepositoryData item);
	void changeRepositoryBookmarkName(RepositoryData item, QString new_name);
	int rowFromCommitId(RepositoryWrapperFrame *frame, const Git::CommitID &id);
	QList<Git::Tag> findTag(RepositoryWrapperFrame *frame, const Git::CommitID &id);
	void sshSetPassphrase(const std::string &user, const std::string &pass);
	std::string sshPassphraseUser() const;
	std::string sshPassphrasePass() const;
	void httpSetAuthentication(const std::string &user, const std::string &pass);
	std::string httpAuthenticationUser() const;
	std::string httpAuthenticationPass() const;
	const Git::CommitItem *getLog(RepositoryWrapperFrame const *frame, int index) const;

	static void updateCommitGraph(Git::CommitItemList *logs);
	void updateCommitGraph(RepositoryWrapperFrame *frame);

	void initNetworking();
	bool saveRepositoryBookmarks(bool update_list);
	QString getBookmarksFilePath() const;
	void stopPtyProcess();
	void abortPtyProcess();
	Git::CommitItemList *getCommitLogPtr(RepositoryWrapperFrame *frame);
	PtyProcess *getPtyProcess();
	bool getPtyProcessOk() const;
	void setCompletedHandler(std::function<void (bool, const QVariant &)> fn, const QVariant &userdata);
	void setPtyProcessOk(bool pty_process_ok);
	const QList<RepositoryData> &cRepositories() const;
	QList<RepositoryData> *pRepositories();
	void setRepositoryList(QList<RepositoryData> &&list);
	bool interactionCanceled() const;
	void setInteractionCanceled(bool canceled);
	InteractionMode interactionMode() const;
	void setInteractionMode(const InteractionMode &im);
	QString getRepositoryFilterText() const;
	void setRepositoryFilterText(const QString &text);
	void setUncommitedChanges(bool uncommited_changes);
	QList<Git::Diff> *diffResult();
	std::map<QString, Git::Diff> *getDiffCacheMap(RepositoryWrapperFrame *frame);
	GitHubRepositoryInfo *ptrGitHub();
	std::map<int, QList<BranchLabel> > *getLabelMap(RepositoryWrapperFrame *frame);
	const std::map<int, QList<BranchLabel> > *getLabelMap(const RepositoryWrapperFrame *frame) const;
	void clearLabelMap(RepositoryWrapperFrame *frame);
	GitObjectCache *getObjCache(RepositoryWrapperFrame *frame);
	bool getForceFetch() const;
	void setForceFetch(bool force_fetch);
	static std::map<Git::CommitID, QList<Git::Tag> > *ptrCommitToTagMap(RepositoryWrapperFrame *frame);
	Git::CommitID getHeadId() const;
	void setHeadId(const Git::CommitID &head_id);
	void setPtyProcessCompletionData(const QVariant &value);
	const QVariant &getTempRepoForCloneCompleteV() const;
	void msgNoRepositorySelected();
	bool isRepositoryOpened() const;
	static std::pair<QString, QString> makeFileItemText(const ObjectData &data);
	QString gitCommand() const;
	QPixmap getTransparentPixmap();
	static QListWidgetItem *newListWidgetFileItem(const MainWindow::ObjectData &data);
	void cancelPendingUserEvents();
	void initRepository(const QString &path, const QString &reponame, const Git::Remote &remote);
	void updatePocessLog(bool processevents);
	void appendLogHistory(const char *ptr, int len);
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
	void customEvent(QEvent *) override;
	void dragEnterEvent(QDragEnterEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;
	bool event(QEvent *event) override;
	bool eventFilter(QObject *watched, QEvent *event) override;
	void internalWriteLog(const char *ptr, int len, bool record);
	RepositoryData const *selectedRepositoryItem() const;
	void removeSelectedRepositoryFromBookmark(bool ask);
public:
	void drawDigit(QPainter *pr, int x, int y, int n) const;
	static constexpr int DIGIT_WIDTH = 5;
	static constexpr int DIGIT_HEIGHT = 7;
	void setStatusBarText(QString const &text);
	void clearStatusBarText();
	void setCurrentLogRow(RepositoryWrapperFrame *frame, int row);
	bool shown();
	void deleteTags(RepositoryWrapperFrame *frame, QStringList const &tagnames);
	void addTag(RepositoryWrapperFrame *frame, QString const &name);
	void updateCurrentFilesList(RepositoryWrapperFrame *frame);
	int selectedLogIndex(RepositoryWrapperFrame *frame, bool lock = true) const;
	void updateAncestorCommitMap(RepositoryWrapperFrame *frame);
	bool isAncestorCommit(const QString &id);
	void postStartEvent(int ms_later);
	void setShowLabels(bool show, bool save);
	void setShowGraph(bool show, bool save);
	bool isLabelsVisible() const;
	bool isGraphVisible() const;
	void updateFileList2(RepositoryWrapperFrame *frame, const Git::CommitID &id, QList<Git::Diff> *diff_list, QListWidget *listwidget);
	void execCommitViewWindow(const Git::CommitItem *commit);
	void execCommitPropertyDialog(QWidget *parent, const Git::CommitItem &commit);
	void execCommitExploreWindow(RepositoryWrapperFrame *frame, QWidget *parent, const Git::CommitItem *commit);
	void execFileHistory(const QString &path);
	void execFileHistory(QListWidgetItem *item);
	void showObjectProperty(QListWidgetItem *item);
	bool testRemoteRepositoryValidity(const QString &url, const QString &sshkey);
	QString selectGitCommand(bool save);
	QString selectGpgCommand(bool save);
	QString selectSshCommand(bool save);
	const Git::Branch &currentBranch() const;
	void setCurrentBranch(const Git::Branch &b);
	const RepositoryData &currentRepository() const;
	QString currentRepositoryName() const;
	QString currentRemoteName() const;
	QString currentBranchName() const;
	GitPtr git(const QString &dir, const QString &submodpath, const QString &sshkey) const;
	GitPtr git();
	GitPtr git(Git::SubmoduleItem const &submod);
	void autoOpenRepository(QString dir, const QString &commit_id = {});
	std::optional<Git::CommitItem> queryCommit(const Git::CommitID &id);
	void checkout(RepositoryWrapperFrame *frame, QWidget *parent, const Git::CommitItem &commit, std::function<void ()> accepted_callback = {});
	void checkout(RepositoryWrapperFrame *frame);
	void jumpToCommit(RepositoryWrapperFrame *frame, const QString &id);
	Git::Object internalCatFile(RepositoryWrapperFrame *frame, const QString &id);
	Git::Object catFile(const QString &id);
	bool saveAs(RepositoryWrapperFrame *frame, const QString &id, const QString &dstpath);
	QString determinFileType(QByteArray const &in) const;
	QString determinFileType(const QString &path) const;
	QList<Git::Tag> queryTagList(RepositoryWrapperFrame *frame);
	static TextEditorThemePtr themeForTextEditor();
    bool isValidWorkingCopy(GitPtr g) const;
	void emitWriteLog(const QByteArray &ba, bool receive);
	QString findFileID(RepositoryWrapperFrame *frame, const QString &commit_id, const QString &file);
	Git::CommitItem commitItem(const RepositoryWrapperFrame *frame, int row) const;
	Git::CommitItem commitItem(const RepositoryWrapperFrame *frame, Git::CommitID const &id) const;
	QImage committerIcon(RepositoryWrapperFrame *frame, int row, QSize size) const;
	void changeSshKey(const QString &local_dir, const QString &ssh_key, bool save);
	static QString abbrevCommitID(const Git::CommitItem &commit);
	const QList<BranchLabel> *label(const RepositoryWrapperFrame *frame, int row) const;
	ApplicationSettings *appsettings();
	const ApplicationSettings *appsettings() const;
	QString defaultWorkingDir() const;
	QIcon signatureVerificationIcon(const Git::CommitID &id) const;
	static QAction *addMenuActionProperty(QMenu *menu);
	QString currentWorkingCopyDir() const;
	Git::SubmoduleItem const *querySubmoduleByPath(const QString &path, Git::CommitItem *commit);
	void refresh();
	bool cloneRepository(const Git::CloneData &clonedata, const RepositoryData &repodata);
	Git::User currentGitUser() const;
	void setupExternalPrograms();
	void updateCommitLogTable(RepositoryWrapperFrame *frame, int delay_ms);
	void updateCommitLogTable(int delay_ms);
private:
	void makeCommitLog(RepositoryWrapperFrame *frame, int scroll_pos, int select_row);
	void setupUpdateCommitLog();
signals:
	void signalUpdateCommitLog();
public:
	void updateCommitLog();

public slots:
	void writeLog_(QByteArray ba, bool receive);
	void onCtrlA();
private slots:
	void updateUI();
	void onLogVisibilityChanged();
	void onRepositoriesTreeDropped();
	void onAvatarUpdated(RepositoryWrapperFrameP frame);
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
	void on_tableWidget_log_currentItemChanged(QTableWidgetItem *current, QTableWidgetItem *previous);
	void on_tableWidget_log_customContextMenuRequested(const QPoint &pos);
	void on_tableWidget_log_itemDoubleClicked(QTableWidgetItem *);
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

	// progress handler
	void onRemoteInfoChanged();
	void onShowProgress(const QString &text, bool cancel_button);
	void onSetProgress(float progress);
	void onHideProgress();
	void onUpdateCommitLog();
	void on_action_rebase_abort_triggered();

	void onShowFileList(FilesListType files_list_type);
	void onAddFileObjectData(const ExchangeData &data);
private:
	void setupProgressHandler() const;
public:
	void setProgress(float progress);
	void showProgress(const QString &text, bool cancel_button);
	void hideProgress();
signals:
	void signalSetProgress(float progress);
	void signalShowProgress(const QString &text, bool cancel_button);
	void signalHideProgress();

	// log handler
protected slots:
	void onLogIdle();
public:
	void writeLog(const char *ptr, int len, bool record);
	void writeLog(std::string_view const &str, bool record);
	void writeLog(const QString &str, bool record);
signals:
	void signalWriteLog(QByteArray ba, bool receive);

private:
	void showFileList(FilesListType files_list_type);
signals:
	void signalShowFileList(FilesListType files_list_type);
private:
	void connectShowFileListHandler();

private:
	void setupAddFileObjectData();
	void addFileObjectData(const ExchangeData &data);

signals:
	void signalAddFileObjectData(const ExchangeData &data);
	void remoteInfoChanged();
private:

	void updateButton();
	void runPtyGit(GitPtr g, std::shared_ptr<AbstractGitCommandItem> params, std::function<void (const ProcessStatus &, const QVariant &)> callback, QVariant const &userdata);
	void queryCommitLog(RepositoryWrapperFrame *frame, GitPtr g);
	void updateHEAD(GitPtr g);
	void jump(GitPtr g, const Git::CommitID &id);
	void jump(GitPtr g, const QString &text);
	void queryTags(GitPtr g);
	void connectPtyProcessCompleted() const;
	void setupShowFileListHandler();
	
	void doReopenRepository(const ProcessStatus &status, const QVariant &userdata);
private slots:
	void onPtyProcessCompleted(bool ok, PtyProcessCompleted const &data);
signals:
	void sigPtyCloneCompleted(bool ok, QVariant const &userdata);
	void sigPtyFetchCompleted(bool ok, QVariant const &userdata);
	void sigPtyProcessCompleted(bool ok, PtyProcessCompleted const &data);
public:
	void internalAfterFetch();
	
};

class ExchangeData {
public:
	RepositoryWrapperFrame *frame = nullptr;
	MainWindow::FilesListType files_list_type;
	std::vector<MainWindow::ObjectData> object_data;
};
Q_DECLARE_METATYPE(ExchangeData)

#endif // MAINWINDOW_H
