#ifndef BASICMAINWINDOW_H
#define BASICMAINWINDOW_H

#include "AvatarLoader.h"
#include "BasicMainWindow.h"
#include "Git.h"
#include "GitHubAPI.h"
#include "GitObjectManager.h"
#include "MyProcess.h"
#include "RepositoryData.h"
#include "TextEditorTheme.h"
#include "main.h"
#include "webclient.h"
#include <QApplication>
#include <QMainWindow>
#include <functional>
#include <memory>

class QListWidget;
class QListWidgetItem;
class QTreeWidgetItem;

struct GitHubRepositoryInfo {
	QString owner_account_name;
	QString repository_name;
};

#define PATH_PREFIX "*"

class BasicMainWindow : public QMainWindow {
	Q_OBJECT
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
protected:
	struct Diff_ {
		QList<Git::Diff> result;
		std::shared_ptr<QThread> thread;
		QList<std::shared_ptr<QThread>> garbage;
	};

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
		IndexRole = Qt::UserRole,
		FilePathRole,
		DiffIndexRole,
		HunkIndexRole,
	};

	enum {
		GroupItem = -1,
	};

	struct Private;
	Private *m;
private:

private:
protected:



	QList<RepositoryItem> repos__;
	QString current_remote_;
	WebContext webcx_;
	AvatarLoader avatar_loader_;
	int update_files_list_counter_ = 0;
	int update_commit_table_counter_ = 0;
	bool interaction_canceled_ = false;
	QString repository_filter_text_;
	bool uncommited_changes_ = false;
	BasicMainWindow::Diff_ diff_;
	std::map<QString, Git::Diff> diff_cache_;
	bool remote_changed_ = false;
	ServerType server_type_ = ServerType::Standard;
	GitHubRepositoryInfo github_;
	std::map<int, QList<Label>> label_map_;
	GitObjectCache objcache_;
	bool force_fetch_ = false;
	std::map<QString, QList<Git::Tag>> tag_map_;
	QString head_id_;
	InteractionMode interaction_mode_ = InteractionMode::None;
	RepositoryItem temp_repo_;
	std::map<QString, QList<Git::Branch>> branch_map_;
private:
private:
	static bool git_callback(void *cookie, const char *ptr, int len);

	QString selectCommand_(const QString &cmdname, const QStringList &cmdfiles, const QStringList &list, QString path, std::function<void (const QString &)> callback);
	QString selectCommand_(const QString &cmdname, const QString &cmdfile, const QStringList &list, QString path, std::function<void (const QString &)> callback);

	bool checkGitCommand();
	bool checkFileCommand();
	bool checkExecutable(const QString &path);
	void internalSetCommand(const QString &path, bool save, const QString &name, QString *out);
	QStringList whichCommand_(const QString &cmdfile1, const QString &cmdfile2 = QString());

	void emitWriteLog(QByteArray ba);

	void setWindowTitle_(const Git::User &user);
	bool execSetGlobalUserDialog();

	bool saveByteArrayAs(const QByteArray &ba, const QString &dstpath);
	bool saveFileAs(const QString &srcpath, const QString &dstpath);
	bool saveBlobAs(const QString &id, const QString &dstpath);

	void updateFilesList(const Git::CommitItem &commit, bool wait);

	void revertAllFiles();


	void deleteTags(const Git::CommitItem &commit);

	bool isAvatarEnabled() const;
	bool isGitHub() const;
	QStringList remotes() const;
	QList<Git::Branch> findBranch(const QString &id);
	QString saveAsTemp(const QString &id);
	QString tempfileHeader() const;
	void deleteTempFiles();
	QString getCommitIdFromTag(const QString &tag);
	bool isValidRemoteURL(const QString &url);
	QString getObjectID(QListWidgetItem *item);
	void addWorkingCopyDir(QString dir, QString name, bool open);
	static QString makeRepositoryName(const QString &loc);
	void clearAuthentication();
	void onAvatarUpdated();
	const RepositoryItem *findRegisteredRepository(QString *workdir) const;
	void queryRemotes(GitPtr g);
	bool runOnRepositoryDir(std::function<void (QString)> callback, const RepositoryItem *repo);
	void clearSshAuthentication();
protected:
	static QString getFilePath(QListWidgetItem *item);
	static bool isGroupItem(QTreeWidgetItem *item);
	static int indexOfLog(QListWidgetItem *item);
	static int indexOfDiff(QListWidgetItem *item);
	static int getHunkIndex(QListWidgetItem *item);

	void initNetworking();
	static QString abbrevCommitID(const Git::CommitItem &commit);
	bool saveRepositoryBookmarks() const;
	QString getBookmarksFilePath() const;

	void stopPtyProcess();
	void abortPtyProcess();

	bool execWelcomeWizardDialog();
	void execRepositoryPropertyDialog(QString workdir, bool open_repository_menu = false);
	void execSetUserDialog(const Git::User &global_user, const Git::User &repo_user, const QString &reponame);
	void setGitCommand(const QString &path, bool save);
	void setFileCommand(const QString &path, bool save);
	void setGpgCommand(const QString &path, bool save);
	void logGitVersion();
	void setUnknownRepositoryInfo();
	void internalClearRepositoryInfo();
	void checkUser();
	void openRepository(bool validate, bool waitcursor = true);
	void reopenRepository(bool log, std::function<void (GitPtr)> callback);
	void openSelectedRepository();
	void checkRemoteUpdate();
	bool isThereUncommitedChanges() const;
	bool makeDiff(QString id, QList<Git::Diff> *out);
	void addDiffItems(const QList<Git::Diff> *diff_list, const std::function<void (const QString &, QString, int)> &add_item);
	Git::CommitItemList retrieveCommitLog(GitPtr g);

	void queryBranches(GitPtr g);
	std::map<QString, QList<Git::Branch>> &branchMapRef();

	void updateCommitTableLater();
	void updateRemoteInfo();
	void updateWindowTitle(GitPtr g);
	QString makeCommitInfoText(int row, QList<Label> *label_list);
	void removeRepositoryFromBookmark(int index, bool ask);

	void clone();
	void checkout();
	void commit(bool amend = false);
	void commitAmend();
	void pushSetUpstream(const QString &remote, const QString &branch);
	bool pushSetUpstream(bool testonly);
	void push();

	void openTerminal(const RepositoryItem *repo);
	void openExplorer(const RepositoryItem *repo);
	const Git::CommitItem *selectedCommitItem() const;
	void deleteBranch(const Git::CommitItem *commit);
	void deleteBranch();
	bool askAreYouSureYouWantToRun(const QString &title, const QString &command);
	bool editFile(const QString &path, const QString &title);
	void resetFile(const QStringList &paths);
	void saveRepositoryBookmark(RepositoryItem item);
	void setCurrentRepository(const RepositoryItem &repo, bool clear_authentication);
	void internalDeleteTags(const QStringList &tagnames);
	bool internalAddTag(const QString &name);
	NamedCommitList namedCommitItems(int flags);
	int rowFromCommitId(const QString &id);
	void createRepository(const QString &dir);
	void setLogEnabled(GitPtr g, bool f);
	QList<Git::Tag> findTag(const QString &id);
	void sshSetPassphrase(const std::string &user, const std::string &pass);
	std::string sshPassphraseUser() const;
	std::string sshPassphrasePass() const;
	void httpSetAuthentication(std::string const &user, std::string const &pass);
	std::string httpAuthenticationUser() const;
	std::string httpAuthenticationPass() const;
public:
	Git::CommitItemList const &getLogs() const;
	const Git::CommitItem *getLog(int index) const;
protected:
	Git::CommitItemList *getLogsPtr();
	void setLogs(const Git::CommitItemList &logs);
	void clearLogs();

	PtyProcess *getPtyProcess();
	PtyCondition getPtyCondition();
	bool getPtyProcessOk() const;
	void setPtyProcessOk(bool pty_process_ok);
	void setPtyCondition(const PtyCondition &ptyCondition);

	QList<RepositoryItem> const &getRepos() const;
	QList<RepositoryItem> *getReposPtr();
protected:
	virtual void setCurrentLogRow(int row) = 0;
	virtual void updateFilesList(QString id, bool wait) = 0;
	virtual void setRepositoryInfo(const QString &reponame, const QString &brname) = 0;
	virtual void deleteTags(const QStringList &tagnames) = 0;
	virtual void internalWriteLog(const char *ptr, int len) = 0;
	virtual void removeSelectedRepositoryFromBookmark(bool ask) = 0;
	virtual void openRepository_(GitPtr g) = 0;
	virtual void updateRepositoriesList() = 0;
	virtual int selectedLogIndex() const = 0;
	virtual void clearFileList() = 0;
	virtual const RepositoryItem *selectedRepositoryItem() const = 0;
	virtual bool isRemoteOnline() const = 0;
public:
	explicit BasicMainWindow(QWidget *parent = nullptr);
	~BasicMainWindow();
	ApplicationSettings *appsettings();
	const ApplicationSettings *appsettings() const;
	WebContext *webContext();
	QString gitCommand() const;
	void autoOpenRepository(QString dir);
	GitPtr git(const QString &dir) const;
	GitPtr git();
	QPixmap getTransparentPixmap();
	QIcon committerIcon(int row) const;
	const Git::CommitItem *commitItem(int row) const;
	QIcon verifiedIcon(char s) const;
	virtual QString currentWorkingCopyDir() const;
	const QList<BasicMainWindow::Label> *label(int row);
	bool saveAs(const QString &id, const QString &dstpath);
	bool testRemoteRepositoryValidity(const QString &url);
	QString defaultWorkingDir() const;
	void addWorkingCopyDir(QString dir, bool open);
	bool queryCommit(const QString &id, Git::CommitItem *out);
	QAction *addMenuActionProperty(QMenu *menu);
	void checkout(QWidget *parent, const Git::CommitItem *commit);
	void jumpToCommit(QString id);
	void execCommitViewWindow(const Git::CommitItem *commit);
	void execCommitExploreWindow(QWidget *parent, const Git::CommitItem *commit);
	void execCommitPropertyDialog(QWidget *parent, const Git::CommitItem *commit);
	void execFileHistory(const QString &path);
	void execFileHistory(QListWidgetItem *item);
	void execFilePropertyDialog(QListWidgetItem *item);
	QString selectGitCommand(bool save);
	QString selectFileCommand(bool save);
	QString selectGpgCommand(bool save);
	const Git::Branch &currentBranch() const;
	void setCurrentBranch(const Git::Branch &b);
	QString currentRepositoryName() const;
	QString currentRemoteName() const;
	QString currentBranchName() const;
	bool isValidWorkingCopy(const GitPtr &g) const;
	QString determinFileType_(const QString &path, bool mime, std::function<void (const QString &, QByteArray *)> callback) const;
	QString determinFileType(const QString &path, bool mime);
	QString determinFileType(QByteArray in, bool mime);
	QList<Git::Tag> queryTagList();
	int limitLogCount() const;
	TextEditorThemePtr themeForTextEditor();
	Git::Object cat_file_(GitPtr g, const QString &id);
	Git::Object cat_file(const QString &id);
	QString newTempFilePath();
	QString findFileID(GitPtr, const QString &commit_id, const QString &file);
	void updateFilesList(QString id, QList<Git::Diff> *diff_list, QListWidget *listwidget);
	void setAppSettings(const ApplicationSettings &appsettings);

	QIcon getRepositoryIcon() const;
	QIcon getFolderIcon() const;
	QIcon getSignatureGoodIcon() const;
	QIcon getSignatureDubiousIcon() const;
	QIcon getSignatureBadIcon() const;
	QPixmap getTransparentPixmap() const;


public slots:
	void writeLog(const char *ptr, int len);
	void writeLog(const QString &str);
	void writeLog(QByteArray ba);
signals:
	void signalWriteLog(QByteArray ba);
	void remoteInfoChanged();
	void signalCheckRemoteUpdate();
};

#endif // BASICMAINWINDOW_H
