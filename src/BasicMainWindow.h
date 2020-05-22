#ifndef BASICMAINWINDOW_H
#define BASICMAINWINDOW_H

#include "Git.h"
#include "GitObjectManager.h"
#include "MyProcess.h"
#include "RepositoryData.h"
#include "TextEditorTheme.h"
#include <QMainWindow>
#include <functional>

class ApplicationSettings;
class AvatarLoader;
class QListWidget;
class QListWidgetItem;
class QTreeWidgetItem;
class WebContext;

struct GitHubRepositoryInfo {
	QString owner_account_name;
	QString repository_name;
};

#define PATH_PREFIX "*"

class BasicMainWindow : public QMainWindow {
	Q_OBJECT
	friend class RepositoryPropertyDialog;
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
		QString info;
		Label(int kind = LocalBranch)
			: kind(kind)
		{

		}
	};
	enum {
		IndexRole = Qt::UserRole,
		FilePathRole,
		DiffIndexRole,
		HunkIndexRole,
		HeaderRole,
	};
protected:

	enum class PtyCondition {
		None,
		Clone,
		Fetch,
		Pull,
		Push,
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

	struct Private;
	Private *m;
private:
	static bool git_callback(void *cookie, const char *ptr, int len);

	QString selectCommand_(QString const &cmdname, QStringList const &cmdfiles, QStringList const &list, QString path, std::function<void (QString const &)> const &callback);
	QString selectCommand_(QString const &cmdname, QString const &cmdfile, QStringList const &list, QString const &path, std::function<void (QString const &)> const &callback);

	bool checkGitCommand();
	bool checkFileCommand();
	bool checkExecutable(QString const &path);
	void internalSetCommand(QString const &path, bool save, QString const &name, QString *out);
	QStringList whichCommand_(QString const &cmdfile1, QString const &cmdfile2 = QString());

	void setWindowTitle_(Git::User const &user);
	bool execSetGlobalUserDialog();

	bool saveByteArrayAs(const QByteArray &ba, QString const &dstpath);
	bool saveFileAs(QString const &srcpath, QString const &dstpath);
	bool saveBlobAs(QString const &id, QString const &dstpath);

	void updateFilesList(Git::CommitItem const &commit, bool wait);

	void revertAllFiles();

	void setCurrentRemoteName(QString const &name);

	void deleteTags(Git::CommitItem const &commit);

	bool isAvatarEnabled() const;
	bool isGitHub() const;
	QStringList remotes() const;
	QList<Git::Branch> findBranch(QString const &id);
	QString saveAsTemp(QString const &id);
	QString tempfileHeader() const;
	void deleteTempFiles();
	QString getCommitIdFromTag(QString const &tag);
	bool isValidRemoteURL(QString const &url, const QString &sshkey);
	QString getObjectID(QListWidgetItem *item);
	void addWorkingCopyDir(QString dir, QString name, bool open);
	static QString makeRepositoryName(QString const &loc);
	void clearAuthentication();
	RepositoryItem const *findRegisteredRepository(QString *workdir) const;
	void queryRemotes(const GitPtr &g);
	bool runOnRepositoryDir(std::function<void (QString)> const &callback, RepositoryItem const *repo);
	void clearSshAuthentication();
protected:
	static QString getFilePath(QListWidgetItem *item);
	static bool isGroupItem(QTreeWidgetItem *item);
	static int indexOfLog(QListWidgetItem *item);
	static int indexOfDiff(QListWidgetItem *item);
	static int getHunkIndex(QListWidgetItem *item);

	void initNetworking();
	bool saveRepositoryBookmarks() const;
	QString getBookmarksFilePath() const;

	void stopPtyProcess();
	void abortPtyProcess();

	bool execWelcomeWizardDialog();
	void execRepositoryPropertyDialog(const RepositoryItem &repo, bool open_repository_menu = false);
	void execSetUserDialog(Git::User const &global_user, Git::User const &repo_user, QString const &reponame);
	void setGitCommand(QString const &path, bool save);
	void setFileCommand(QString const &path, bool save);
	void setGpgCommand(QString const &path, bool save);
	void setSshCommand(QString const &path, bool save);
	void logGitVersion();
	void setUnknownRepositoryInfo();
	void internalClearRepositoryInfo();
	void checkUser();
	void openRepository(bool validate, bool waitcursor = true, bool keep_selection = false);
	void updateRepository();
	void reopenRepository(bool log, const std::function<void(GitPtr const &)> &callback);
	void openSelectedRepository();
//	void checkRemoteUpdate();
	bool isThereUncommitedChanges() const;
	bool makeDiff(QString id, QList<Git::Diff> *out);
	void addDiffItems(const QList<Git::Diff> *diff_list, const std::function<void (QString const &, QString, int)> &add_item);
	Git::CommitItemList retrieveCommitLog(const GitPtr &g);

	void queryBranches(const GitPtr &g);
	std::map<QString, QList<Git::Branch>> &branchMapRef();

	void updateCommitTableLater();
	void updateRemoteInfo();
	void updateWindowTitle(const GitPtr &g);
	QString makeCommitInfoText(int row, QList<Label> *label_list);
	void removeRepositoryFromBookmark(int index, bool ask);

	void clone(QString url = QString(), QString dir = QString());
	void checkout();
	void commit(bool amend = false);
	void commitAmend();
	void pushSetUpstream(QString const &remote, QString const &branch);
	bool pushSetUpstream(bool testonly);
	void push();

	void openTerminal(RepositoryItem const *repo);
	void openExplorer(RepositoryItem const *repo);
	Git::CommitItem const *selectedCommitItem() const;
	void deleteBranch(Git::CommitItem const *commit);
	void deleteBranch();
	bool askAreYouSureYouWantToRun(QString const &title, QString const &command);
	bool editFile(QString const &path, QString const &title);
	void resetFile(QStringList const &paths);
	void saveRepositoryBookmark(RepositoryItem item);
	void setCurrentRepository(RepositoryItem const &repo, bool clear_authentication);
	void internalDeleteTags(QStringList const &tagnames);
	bool internalAddTag(QString const &name);
	NamedCommitList namedCommitItems(int flags);
	int rowFromCommitId(QString const &id);
	void createRepository(QString const &dir);
	void setLogEnabled(const GitPtr &g, bool f);
	QList<Git::Tag> findTag(QString const &id);
	void sshSetPassphrase(std::string const &user, std::string const &pass);
	std::string sshPassphraseUser() const;
	std::string sshPassphrasePass() const;
	void httpSetAuthentication(std::string const &user, std::string const &pass);
	std::string httpAuthenticationUser() const;
	std::string httpAuthenticationPass() const;
public:
	Git::CommitItemList const &getLogs() const;
	Git::CommitItem const *getLog(int index) const;
protected:
	bool event(QEvent *event) override;
	void postUserFunctionEvent(const std::function<void(QVariant const &)> &fn, QVariant const &v = QVariant());

	void doGitCommand(std::function<void(GitPtr)> const &callback);

	Git::CommitItemList *getLogsPtr();
	void setLogs(const Git::CommitItemList &logs);
	void clearLogs();

	PtyProcess *getPtyProcess();
	PtyCondition getPtyCondition();
	bool getPtyProcessOk() const;
	void setPtyProcessOk(bool pty_process_ok);
	void setPtyUserData(QVariant const &userdata);
	void setPtyCondition(const PtyCondition &ptyCondition);

	QList<RepositoryItem> const &getRepos() const;
	QList<RepositoryItem> *getReposPtr();

	QString getCurrentRemoteName() const;

	AvatarLoader *getAvatarLoader();
	const AvatarLoader *getAvatarLoader() const;

	int *ptrUpdateFilesListCounter();
	int *ptrUpdateCommitTableCounter();

	bool interactionCanceled() const;
	void setInteractionCanceled(bool canceled);

	InteractionMode interactionMode() const;
	void setInteractionMode(const InteractionMode &im);

	QString getRepositoryFilterText() const;
	void setRepositoryFilterText(QString const &text);

	void setUncommitedChanges(bool uncommited_changes);

	QList<Git::Diff> *diffResult();
	std::map<QString, Git::Diff> *getDiffCacheMap();

	bool getRemoteChanged() const;
	void setRemoteChanged(bool remote_changed);

	void setServerType(const ServerType &server_type);

	GitHubRepositoryInfo *ptrGitHub();

	std::map<int, QList<Label>> *getLabelMap();
	const std::map<int, QList<Label>> *getLabelMap() const;
	void clearLabelMap();

	GitObjectCache *getObjCache();

	bool getForceFetch() const;
	void setForceFetch(bool force_fetch);

	std::map<QString, QList<Git::Tag>> *ptrTagMap();

	QString getHeadId() const;
	void setHeadId(QString const &head_id);

	void setPtyProcessCompletionData(QVariant const &value);
	QVariant const &getTempRepoForCloneCompleteV() const;

	void updateCommitGraph();
	bool fetch(const GitPtr &g, bool prune);
	bool fetch_tags_f(const GitPtr &g);

protected:
	virtual void setCurrentLogRow(int row) = 0;
	virtual void updateFilesList(QString id, bool wait) = 0;
	virtual void setRepositoryInfo(QString const &reponame, QString const &brname) = 0;
	virtual void deleteTags(QStringList const &tagnames) = 0;
	virtual void internalWriteLog(const char *ptr, int len) = 0;
	virtual void removeSelectedRepositoryFromBookmark(bool ask) = 0;
	virtual void openRepository_(GitPtr g, bool keep_selection = false) = 0;
	virtual void updateRepositoriesList() = 0;
	virtual void clearFileList() = 0;
	virtual RepositoryItem const *selectedRepositoryItem() const = 0;
	virtual void setRemoteMonitoringEnabled(bool enable) { (void)enable; }
	virtual void updateStatusBarText() {}
	void msgNoRepositorySelected();
	bool isRepositoryOpened() const;
public:
	explicit BasicMainWindow(QWidget *parent = nullptr);
	~BasicMainWindow() override;
	ApplicationSettings *appsettings();
	const ApplicationSettings *appsettings() const;
	WebContext *webContext();
	QString gitCommand() const;
	void autoOpenRepository(QString dir);
	GitPtr git(QString const &dir, const QString &sshkey = {}) const;
	GitPtr git();
	QPixmap getTransparentPixmap();
	QIcon committerIcon(int row) const;
	Git::CommitItem const *commitItem(int row) const;
	QIcon verifiedIcon(char s) const;
	virtual QString currentWorkingCopyDir() const;
	const QList<Label> *label(int row) const;
	bool saveAs(QString const &id, QString const &dstpath);
	bool testRemoteRepositoryValidity(QString const &url, const QString &sshkey);
	QString defaultWorkingDir() const;
	void addWorkingCopyDir(const QString &dir, bool open);
	bool queryCommit(QString const &id, Git::CommitItem *out);
	QAction *addMenuActionProperty(QMenu *menu);
	void checkout(QWidget *parent, Git::CommitItem const *commit, std::function<void()> accepted_callback = nullptr);
	void jumpToCommit(QString id);
	void execCommitViewWindow(Git::CommitItem const *commit);
	void execCommitExploreWindow(QWidget *parent, Git::CommitItem const *commit);
	void execCommitPropertyDialog(QWidget *parent, Git::CommitItem const *commit);
	void execFileHistory(QString const &path);
	void execFileHistory(QListWidgetItem *item);
	void execFilePropertyDialog(QListWidgetItem *item);
	QString selectGitCommand(bool save);
	QString selectFileCommand(bool save);
	QString selectGpgCommand(bool save);
	QString selectSshCommand(bool save);
	Git::Branch const &currentBranch() const;
	void setCurrentBranch(Git::Branch const &b);
	const RepositoryItem &currentRepository() const;
	QString currentRepositoryName() const;
	QString currentRemoteName() const;
	QString currentBranchName() const;
	bool isValidWorkingCopy(const GitPtr &g) const;
	QString determinFileType_(QString const &path, bool mime, std::function<void (QString const &, QByteArray *)> const &callback) const;
	QString determinFileType(QString const &path, bool mime);
	QString determinFileType(QByteArray in, bool mime);
	QList<Git::Tag> queryTagList();
	int limitLogCount() const;
	TextEditorThemePtr themeForTextEditor();
	Git::Object cat_file_(const GitPtr &g, QString const &id);
	Git::Object cat_file(QString const &id);
	QString newTempFilePath();
	QString findFileID(QString const &commit_id, QString const &file);
	void updateFilesList(QString const &id, QList<Git::Diff> *diff_list, QListWidget *listwidget);
	void setAppSettings(const ApplicationSettings &appsettings);

	QIcon getRepositoryIcon() const;
	QIcon getFolderIcon() const;
	QIcon getSignatureGoodIcon() const;
	QIcon getSignatureDubiousIcon() const;
	QIcon getSignatureBadIcon() const;
	QPixmap getTransparentPixmap() const;

	static QString abbrevCommitID(Git::CommitItem const &commit);
	QStringList findGitObject(const QString &id) const;

	void writeLog(const char *ptr, int len);
	void writeLog(QString const &str);
	void emitWriteLog(const QByteArray &ba);
	QList<Label> sortedLabels(int row) const;

	virtual bool isOnlineMode() const = 0;
	virtual int selectedLogIndex() const = 0;
	void changeSshKey(const QString &localdir, const QString &sshkey);
	void saveApplicationSettings();
protected slots:
	void onAvatarUpdated();
public slots:
	void writeLog_(QByteArray ba);
signals:
	void signalWriteLog(QByteArray ba);
	void remoteInfoChanged();
//	void signalCheckRemoteUpdate();
};

#endif // BASICMAINWINDOW_H
