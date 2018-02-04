#ifndef GIT_H
#define GIT_H

#include "AbstractProcess.h"

#include <QDateTime>
#include <QObject>
#include <functional>

#include <QDebug>
#include <QMutex>
#include <memory>

#define SINGLE_THREAD 0

#define GIT_ID_LENGTH (40)

class Win32PtyProcess;

enum class LineSide {
	Left,
	Right,
};

struct TreeLine {
	int index;
	int depth;
	int color_number = 0;
	bool bend_early = false;
	TreeLine(int index = -1, int depth = -1)
		: index(index)
		, depth(depth)
	{
	}
};

struct NamedCommitItem {
	enum class Type {
		None,
		Branch,
		Tag,
	};
	Type type = Type::None;
	QString name;
	QString id;
};
typedef QList<NamedCommitItem> NamedCommitList;


class Git;
typedef std::shared_ptr<Git> GitPtr;

class Git : QObject {
public:
	class Context {
	public:
		QString git_command;
	};

	struct Object {
		enum class Type {
			UNKNOWN = 0,
			COMMIT = 1,
			TREE = 2,
			BLOB = 3,
			TAG = 4,
			UNDEFINED = 5,
			OFS_DELTA = 6,
			REF_DELTA = 7,
		};
		Type type = Type::UNKNOWN;
		QByteArray content;
	};

	class Hunk {
	public:
		std::string at;
		std::vector<std::string> lines;
	};
	class Diff {
	public:
		enum class Type {
			Unknown,
			Modify,
			Copy,
			Rename,
			Create,
			Delete,
			ChType,
			Unmerged,
		};
		Type type = Type::Unknown;
		QString diff;
		QString index;
		QString path;
		QString mode;
		struct BLOB_AB_ {
			QString a_id;
			QString b_id;
		} blob;
		QList<Hunk> hunks;
		Diff()
		{
		}
		Diff(const QString &id, const QString &path, const QString &mode)
		{
			makeForSingleFile(this, id, path, mode);

		}
	private:
		void makeForSingleFile(Git::Diff *diff, const QString &id, const QString &path, const QString &mode);
	};

	struct CommitItem {
		QString commit_id;
		QStringList parent_ids;
		QString author;
		QString email;
		QString message;
		QDateTime commit_date;
		std::vector<TreeLine> parent_lines;
		bool has_child = false;
		int marker_depth = -1;
		bool resolved =  false;

	};
	typedef std::vector<CommitItem> CommitItemList;

	static bool isUncommited(CommitItem const &item)
	{
		return item.commit_id.isEmpty();
	}

	struct Branch {
		QString name;
		QString id;
		int ahead = 0;
		int behind = 0;
		enum {
			None,
			Current = 0x0001,
			HeadDetached = 0x0002,
		};
		int flags = 0;
		bool isHeadDetached() const
		{
			return flags & HeadDetached;
		}
	};

	struct Tag {
		QString name;
		QString id;
	};

	enum class FileStatusCode : unsigned int {
		Unknown,
		Ignored,
		Untracked,
		NotUpdated = 0x10000000,
		Staged_ = 0x20000000,
		UpdatedInIndex,
		AddedToIndex,
		DeletedFromIndex,
		RenamedInIndex,
		CopiedInIndex,
		Unmerged_ = 0x40000000,
		Unmerged_BothDeleted,
		Unmerged_AddedByUs,
		Unmerged_DeletedByThem,
		Unmerged_AddedByThem,
		Unmerged_DeletedByUs,
		Unmerged_BothAdded,
		Unmerged_BothModified,
		Tracked_ = 0xf0000000
	};

	class FileStatus {
	public:
		struct Data {
			char code_x = 0;
			char code_y = 0;
			FileStatusCode code = FileStatusCode::Unknown;
			QString rawpath1;
			QString rawpath2;
			QString path1;
			QString path2;
		} data;

		static FileStatusCode parseFileStatusCode(char x, char y);

		bool isStaged() const
		{
			return (int)data.code & (int)FileStatusCode::Staged_;
		}

		bool isUnmerged() const
		{
			return (int)data.code & (int)FileStatusCode::Unmerged_;
		}

		bool isTracked() const
		{
			return (int)data.code & (int)FileStatusCode::Tracked_;
		}

		void parse(QString const &text);

		FileStatus()
		{
		}

		FileStatus(QString const &text)
		{
			parse(text);
		}

		FileStatusCode code() const
		{
			return data.code;
		}

		int code_x() const
		{
			return data.code_x;
		}

		int code_y() const
		{
			return data.code_y;
		}

		bool isDeleted() const
		{
			return code_x() == 'D' || code_y() == 'D';
		}

		QString path1() const
		{
			return data.path1;
		}

		QString path2() const
		{
			return data.path2;
		}

		QString rawpath1() const
		{
			return data.rawpath1;
		}

		QString rawpath2() const
		{
			return data.rawpath2;
		}
	};
	typedef std::vector<FileStatus> FileStatusList;

	static QString trimPath(const QString &s);

private:
	struct Private;
	Private *m;
	QStringList make_branch_list_();
	QByteArray cat_file_(const QString &id);
	FileStatusList status_();
	bool commit_(const QString &msg, bool amend);
	bool push_(bool tags, AbstractPtyProcess *pty);
	static void parseAheadBehind(const QString &s, Branch *b);
	Git();
	QString encodeQuotedText(const QString &str);
	QStringList refs();
public:
	Git(Context const &cx, const QString &repodir);
	Git(Git &&r) = delete;
	virtual ~Git();

	typedef bool (*callback_t)(void *cookie, char const *ptr, int len);

	void setLogCallback(callback_t func, void *cookie);

	QByteArray toQByteArray() const;
	void setGitCommand(const QString &path);
	QString gitCommand() const;
	void clearResult();
	QString resultText() const;
	bool chdirexec(std::function<bool ()> fn);
	bool git(QString const &arg, bool chdir, bool errout = false, AbstractPtyProcess *pty = nullptr);
	bool git(QString const &arg)
	{
		return git(arg, true);
	}

	void setWorkingRepositoryDir(const QString &repo);
	const QString &workingRepositoryDir() const;

	QString getCurrentBranchName();
	bool isValidWorkingCopy() const;
	QString version();
	bool init();
	QStringList getUntrackedFiles();
	CommitItemList log_all(const QString &id, int maxcount);
	CommitItemList log(int maxcount);
	bool queryCommit(const QString &id, CommitItem *out);

	struct CloneData {
		QString url;
		QString basedir;
		QString subdir;
	};
	static CloneData preclone(QString const &url, QString const &path);
	bool clone(CloneData const &data, AbstractPtyProcess *pty);

	FileStatusList status();
	bool cat_file(const QString &id, QByteArray *out);
	void resetFile(const QString &path);
	void resetAllFiles();

	void removeFile(const QString &path);

	void stage(const QString &path);
	void stage(const QStringList &paths);
	void unstage(const QString &path);
	void unstage(const QStringList &paths);
	void pull(AbstractPtyProcess *pty = 0);

	void fetch(AbstractPtyProcess *pty = 0);

	QList<Branch> branches_();
	QList<Branch> branches();

	int getProcessExitCode() const;

	QString diff(QString const &old_id, QString const &new_id);

	struct DiffRaw {
		struct AB {
			QString id;
			QString mode;
		} a, b;
		QString state;
		QStringList files;
	};

	struct Remote {
		QString remote;
		QString url;
		QString purpose;
	};

	QList<DiffRaw> diff_raw(const QString &old_id, const QString &new_id);

	static bool isValidID(QString const &s);

	bool commit(const QString &text);
	bool commit_amend_m(const QString &text);
	bool revert(const QString &id);
	bool push(bool tags, AbstractPtyProcess *pty = 0);
	void getRemoteURLs(QList<Remote> *out);
	void createBranch(const QString &name);
	void checkoutBranch(const QString &name);
	void mergeBranch(const QString &name);
	static bool isValidWorkingCopy(const QString &dir);
	QString diff_to_file(const QString &old_id, const QString &path);
	QString errorMessage() const;

	GitPtr dup() const;
	QString rev_parse(const QString &name);
	QList<Tag> tags();
	void tag(const QString &name, QString const &id = QString());
	void delete_tag(const QString &name, bool remote);
	void setRemoteURL(const QString &remote, const QString &url);
	void addRemoteURL(QString const &remote, QString const &url);
	QStringList getRemotes();

	struct User {
		QString name;
		QString email;
	};
	enum GetUser {
		GetUserDefault,
		GetUserGlobal,
		GetUserLocal,
	};

	User getUser(GetUser purpose);
	void setUser(User const&user, bool global);

	bool reset_head1();
	void push_u(const QString &remote, const QString &branch);
	void push_u_origin_master();
	QString objectType(const QString &id);
	bool rm_cached(const QString &file);
	void cherrypick(const QString &name);

	struct ReflogItem {
		QString id;
		QString head;
		QString command;
		QString comment;
		struct File {
			QString atts_a;
			QString atts_b;
			QString id_a;
			QString id_b;
			QString type;
			QString path;
		};
		QList<File> files;
	};
	typedef QList<ReflogItem> ReflogItemList;

	bool reflog(ReflogItemList *out, int maxcount = 100);
	QByteArray blame(const QString &path);
};

#endif // GIT_H
