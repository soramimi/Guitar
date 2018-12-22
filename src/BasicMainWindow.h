#ifndef BASICMAINWINDOW_H
#define BASICMAINWINDOW_H

#include <QMainWindow>
#include "BasicMainWindow.h"
#include "Git.h"
#include "RepositoryData.h"
#include "GitHubAPI.h"
#include "main.h"
#include "TextEditorTheme.h"
#include "GitObjectManager.h"
#include "webclient.h"
#include "AvatarLoader.h"
#include <functional>
#include <memory>
#include "MyProcess.h"

class QListWidget;
class QListWidgetItem;

struct GitHubRepositoryInfo {
	QString owner_account_name;
	QString repository_name;
};


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


	QString starting_dir_;
	Git::Context gcx_;
	ApplicationSettings appsettings_;
	QList<RepositoryItem> repos_;
	RepositoryItem current_repo_;
	ServerType server_type_ = ServerType::Standard;
	GitHubRepositoryInfo github_;
	QString current_remote_;
	Git::Branch current_branch_;
	QString head_id_;
	BasicMainWindow::Diff_ diff_;
	std::map<QString, Git::Diff> diff_cache_;
	QStringList added_;
	Git::CommitItemList logs_;
	bool force_fetch_ = false;
	bool uncommited_changes_ = false;
	int update_files_list_counter_ = 0;
	std::map<int, QList<Label>> label_map_;
	QStringList remotes_;
	std::map<QString, QList<Git::Branch>> branch_map_;
	std::map<QString, QList<Git::Tag>> tag_map_;
	QString repository_filter_text_;
	unsigned int temp_file_counter_ = 0;
	GitObjectCache objcache_;

	WebContext webcx_;
	AvatarLoader avatar_loader_;
	int update_commit_table_counter_ = 0;

	std::map<QString, GitHubAPI::User> committer_map_; // key is email

	PtyProcess pty_process_;
	PtyCondition pty_condition_ = PtyCondition::None;
	bool pty_process_ok_ = false;

	RepositoryItem temp_repo_;
	InteractionMode interaction_mode_ = InteractionMode::None;
	bool interaction_canceled_ = false;

	bool remote_changed_ = false;

	std::string ssh_passphrase_for_;
	std::string ssh_passphrase_secret_;

	std::string http_uid_;
	std::string http_pwd_;

	QIcon repository_icon_;
	QIcon folder_icon_;
	QIcon signature_good_icon_;
	QIcon signature_dubious_icon_;
	QIcon signature_bad_icon_;

	QPixmap transparent_pixmap_;

	QString selectCommand_(const QString &cmdname, const QStringList &cmdfiles, const QStringList &list, QString path, std::function<void (const QString &)> callback);
	QString selectCommand_(const QString &cmdname, const QString &cmdfile, const QStringList &list, QString path, std::function<void (const QString &)> callback);
	bool checkGitCommand();
	bool checkFileCommand();
	static bool git_callback(void *cookie, const char *ptr, int len);
	void emitWriteLog(QByteArray ba);
	void internalSetCommand(const QString &path, bool save, const QString &name, QString *out);
	virtual void internalWriteLog(const char *ptr, int len) = 0;
	QStringList whichCommand_(const QString &cmdfile1, const QString &cmdfile2 = QString());
	void checkUser();
	bool execSetGlobalUserDialog();
	void setWindowTitle_(const Git::User &user);
	void setUnknownRepositoryInfo();
	virtual void setRepositoryInfo(const QString &reponame, const QString &brname) = 0;
	void updateWindowTitle(GitPtr g);
	Git::CommitItemList retrieveCommitLog(GitPtr g);
	void initNetworking();
	bool saveByteArrayAs(const QByteArray &ba, const QString &dstpath);
	bool saveFileAs(const QString &srcpath, const QString &dstpath);
	bool saveBlobAs(const QString &id, const QString &dstpath);

	void openRepository(bool validate, bool waitcursor = true);
	virtual void removeSelectedRepositoryFromBookmark(bool ask) = 0;
	virtual void openRepository_(GitPtr g) = 0;
	virtual void setCurrentLogRow(int row) = 0;
	bool saveRepositoryBookmarks() const;
	QString getBookmarksFilePath() const;
	virtual void updateRepositoriesList() = 0;
	virtual int selectedLogIndex() const = 0;
	virtual void clearFileList() = 0;
	virtual void updateFilesList(QString id, bool wait) = 0;
	void updateFilesList(const Git::CommitItem &commit, bool wait);
	void reopenRepository(bool log, std::function<void (GitPtr)> callback);
	void openSelectedRepository();
	bool askAreYouSureYouWantToRun(const QString &title, const QString &command);
	void resetFile(const QStringList &paths);
	void revertAllFiles();
	bool editFile(const QString &path, const QString &title);
	virtual const RepositoryItem *selectedRepositoryItem() const = 0;
	void internalDeleteTags(const QStringList &tagnames);
	void removeRepositoryFromBookmark(int index, bool ask);
	void deleteTags(const Git::CommitItem &commit);
	virtual void deleteTags(const QStringList &tagnames) = 0;
	bool internalAddTag(const QString &name);
	bool makeDiff(QString id, QList<Git::Diff> *out);
public:
	explicit BasicMainWindow(QWidget *parent = nullptr);
	WebContext *webContext();
	QString currentRepositoryName() const;
	QString currentRemoteName() const;
	const Git::Branch &currentBranch() const;
	QString currentBranchName() const;
	virtual QString currentWorkingCopyDir() const;
	GitPtr git(const QString &dir) const;
	GitPtr git();
	QString selectGitCommand(bool save);
	bool execWelcomeWizardDialog();
	ApplicationSettings *appsettings();
	const ApplicationSettings *appsettings() const;
	QString selectFileCommand(bool save);
	QString selectGpgCommand(bool save);
	bool checkExecutable(const QString &path);
	void setGitCommand(const QString &path, bool save);
	void setFileCommand(const QString &path, bool save);
	void setGpgCommand(const QString &path, bool save);
	bool isValidWorkingCopy(const GitPtr &g) const;
	bool isAvatarEnabled() const;
	void updateCommitTableLater();
	bool isGitHub() const;
	const Git::CommitItem *commitItem(int row) const;
	QIcon committerIcon(int row) const;
	const QList<BasicMainWindow::Label> *label(int row);
	QString makeCommitInfoText(int row, QList<Label> *label_list);
	QStringList remotes() const;
	int limitLogCount() const;
	QList<Git::Branch> findBranch(const QString &id);
	QList<Git::Tag> findTag(const QString &id);
	void logGitVersion();
	bool saveAs(const QString &id, const QString &dstpath);
	QString saveAsTemp(const QString &id);
	QString newTempFilePath();
	QString tempfileHeader() const;
	void deleteTempFiles();
	QString determinFileType_(const QString &path, bool mime, std::function<void (const QString &, QByteArray *)> callback) const;
	QString determinFileType(const QString &path, bool mime);
	QString determinFileType(QByteArray in, bool mime);
	void execFilePropertyDialog(QListWidgetItem *item);
	QString getCommitIdFromTag(const QString &tag);
	bool isValidRemoteURL(const QString &url);
	bool testRemoteRepositoryValidity(const QString &url);
	void execSetUserDialog(const Git::User &global_user, const Git::User &repo_user, const QString &reponame);
	NamedCommitList namedCommitItems(int flags);
	QString getObjectID(QListWidgetItem *item);
	void stopPtyProcess();
	void abortPtyProcess();
	void jumpToCommit(QString id);
	int rowFromCommitId(const QString &id);
	void execFileHistory(const QString &path);
	void execFileHistory(QListWidgetItem *item);
	void addWorkingCopyDir(QString dir, QString name, bool open);
	void addWorkingCopyDir(QString dir, bool open);
	void setLogEnabled(GitPtr g, bool f);
	QString findFileID(GitPtr, const QString &commit_id, const QString &file);
	static QString abbrevCommitID(const Git::CommitItem &commit);
	static QString makeRepositoryName(const QString &loc);
	void saveRepositoryBookmark(RepositoryItem item);
	void setCurrentRepository(const RepositoryItem &repo, bool clear_authentication);
	void clearAuthentication();
	TextEditorThemePtr themeForTextEditor();
	Git::Object cat_file_(GitPtr g, const QString &id);
	Git::Object cat_file(const QString &id);
	void execCommitPropertyDialog(QWidget *parent, const Git::CommitItem *commit);
	QIcon verifiedIcon(char s) const;
	bool queryCommit(const QString &id, Git::CommitItem *out);
	void checkout(QWidget *parent, const Git::CommitItem *commit);
	void checkout();
	const Git::CommitItem *selectedCommitItem() const;
	void execCommitExploreWindow(QWidget *parent, const Git::CommitItem *commit);
	void execCommitViewWindow(const Git::CommitItem *commit);
	QAction *addMenuActionProperty(QMenu *menu);
	QPixmap getTransparentPixmap();
	void updateFilesList(QString id, QList<Git::Diff> *diff_list, QListWidget *listwidget);
	void addDiffItems(const QList<Git::Diff> *diff_list, const std::function<void (const QString &, QString, int)> &add_item);
	void onAvatarUpdated();
	QList<Git::Tag> queryTagList();
	void execRepositoryPropertyDialog(QString workdir, bool open_repository_menu = false);
	const RepositoryItem *findRegisteredRepository(QString *workdir) const;
	bool isThereUncommitedChanges() const;
	void commit(bool amend = false);
	void commitAmend();
	void queryBranches(GitPtr g);
	void updateRemoteInfo();
	void queryRemotes(GitPtr g);
	void deleteBranch(const Git::CommitItem *commit);
	void clone();
	virtual bool isRemoteOnline() const = 0;
	void deleteBranch();
	bool runOnRepositoryDir(std::function<void (QString)> callback, const RepositoryItem *repo);
	QString defaultWorkingDir() const;
	void autoOpenRepository(QString dir);
	void openTerminal(const RepositoryItem *repo);
	void openExplorer(const RepositoryItem *repo);
	void pushSetUpstream(const QString &remote, const QString &branch);
	bool pushSetUpstream(bool testonly);
	void push();
	void createRepository(const QString &dir);
	void checkRemoteUpdate();
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
