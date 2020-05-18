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
		BranchLocal,
		BranchRemote,
		Tag,
	};
	Type type = Type::None;
    QString remote;
	QString name;
	QString id;
};
using NamedCommitList = QList<NamedCommitItem>;

class Git;
using GitPtr = std::shared_ptr<Git>;

class Git : QObject {
public:
	class Context {
	public:
		QString git_command;
		QString ssh_command;// = "C:/Program Files/Git/usr/bin/ssh.exe";
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
		Diff() = default;
		Diff(QString const &id, QString const &path, QString const &mode)
		{
			makeForSingleFile(this, QString(GIT_ID_LENGTH, '0'), id, path, mode);
		}
	private:
		void makeForSingleFile(Git::Diff *diff, QString const &id_a, QString const &id_b, QString const &path, QString const &mode);
	};

	enum class SignatureGrade {
		NoSignature,
		Unknown,
		Good,
		Dubious,
		Missing,
		Bad,
	};

	static SignatureGrade evaluateSignature(char s)
	{
		switch (s) {
		case 'G':
			return SignatureGrade::Good;
		case 'U':
		case 'X':
		case 'Y':
			return SignatureGrade::Dubious;
		case 'B':
		case 'R':
			return SignatureGrade::Bad;
		case 'E':
			return SignatureGrade::Missing;
		case 'N':
		case ' ':
		case 0:
			return SignatureGrade::NoSignature;
		}
		return SignatureGrade::Unknown;
	}

	struct CommitItem {
		QString commit_id;
		QStringList parent_ids;
		QString author;
		QString email;
		QString message;
		QDateTime commit_date;
		std::vector<TreeLine> parent_lines;
		QByteArray fingerprint;
		char signature = 0; // git log format:%G?
		bool has_child = false;
		int marker_depth = -1;
		bool resolved =  false;
		bool strange_date = false;
	};
	using CommitItemList = std::vector<CommitItem>;

	static bool isUncommited(CommitItem const &item)
	{
		return item.commit_id.isEmpty();
	}

	struct Branch {
		QString name;
		QString id;
		QString remote;
		int ahead = 0;
		int behind = 0;
		enum {
			None,
			Current = 0x0001,
			HeadDetachedAt = 0x0002,
			HeadDetachedFrom = 0x0004,
		};
		int flags = 0;
		operator bool () const
		{
			if (name.isEmpty()) return false;
			if (id.isEmpty()) return false;
			return true;
		}
		bool isCurrent() const
		{
			return flags & Current;
		}
		bool isHeadDetached() const
		{
			return flags & HeadDetachedAt;
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

	enum class MergeFastForward {
		Default,
		No,
		Only,
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

		FileStatus() = default;

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
	using FileStatusList = std::vector<FileStatus>;

	static QString trimPath(QString const &s);

private:
	struct Private;
	Private *m;
	QStringList make_branch_list_();
	QByteArray cat_file_(QString const &id);
	FileStatusList status_s_();
	bool commit_(QString const &msg, bool amend, bool sign, AbstractPtyProcess *pty);
	bool push_(bool tags, AbstractPtyProcess *pty);
	static void parseAheadBehind(QString const &s, Branch *b);
	Git();
	QString encodeQuotedText(QString const &str);
public:
	Git(Context const &cx, QString const &repodir, QString const &sshkey = {});
	Git(Git &&r) = delete;
	 ~Git() override;

	using callback_t = bool (*)(void *, const char *, int);

	void setLogCallback(callback_t func, void *cookie);

	QByteArray toQByteArray() const;
	void setGitCommand(QString const &gitcmd, const QString &sshcmd = {});
	QString gitCommand() const;
	void clearResult();
	QString resultText() const;
	bool chdirexec(std::function<bool ()> const &fn);
	bool git(QString const &arg, bool chdir, bool errout = false, AbstractPtyProcess *pty = nullptr);
	bool git(QString const &arg)
	{
		return git(arg, true);
	}

	void setWorkingRepositoryDir(QString const &repo, const QString &sshkey);
	QString const &workingRepositoryDir() const;
	QString const &sshKey() const;
	void setSshKey(const QString &sshkey) const;

	QString getCurrentBranchName();
	bool isValidWorkingCopy() const;
	QString version();
	bool init();
	QStringList getUntrackedFiles();
	CommitItemList log_all(QString const &id, int maxcount);
	CommitItemList log(int maxcount);
	bool queryCommit(QString const &id, CommitItem *out);

	struct CloneData {
		QString url;
		QString basedir;
		QString subdir;
	};
	static CloneData preclone(QString const &url, QString const &path);
	bool clone(CloneData const &data, AbstractPtyProcess *pty);

	FileStatusList status_s();
	bool cat_file(QString const &id, QByteArray *out);
	void resetFile(QString const &path);
	void resetAllFiles();

	void removeFile(QString const &path);

	void stage(QString const &path);
	void stage(QStringList const &paths);
	void unstage(QString const &path);
	void unstage(QStringList const &paths);
	void pull(AbstractPtyProcess *pty = nullptr);

	void fetch(AbstractPtyProcess *pty = nullptr, bool prune = false);
	void fetch_tags_f(AbstractPtyProcess *pty);

	QList<Branch> branches();

	int getProcessExitCode() const;

	QString diff(QString const &old_id, QString const &new_id);
	QString diff_file(QString const &old_path, QString const &new_path);

	struct DiffRaw {
		struct AB {
			QString id;
			QString mode;
		} a, b;
		QString state;
		QStringList files;
	};

	struct Remote {
		QString name;
		QString url;
		QString purpose;
		QString ssh_key;
	};

	QList<DiffRaw> diff_raw(QString const &old_id, QString const &new_id);

	static bool isValidID(QString const &id);

	QString status();
	bool commit(QString const &text, bool sign, AbstractPtyProcess *pty);
	bool commit_amend_m(QString const &text, bool sign, AbstractPtyProcess *pty);
	bool revert(QString const &id);
	bool push(bool tags, AbstractPtyProcess *pty = nullptr);
	void getRemoteURLs(QList<Remote> *out);
	void createBranch(QString const &name);
	void checkoutBranch(QString const &name);
	void mergeBranch(QString const &name, MergeFastForward ff);
	void rebaseBranch(QString const &name);
	static bool isValidWorkingCopy(QString const &dir);
	QString diff_to_file(QString const &old_id, QString const &path);
	QString errorMessage() const;

	GitPtr dup() const;
	QString rev_parse(QString const &name);
	QList<Tag> tags();
	QList<Tag> tags2();
	bool tag(QString const &name, QString const &id = QString());
	void delete_tag(QString const &name, bool remote);
	void setRemoteURL(const Remote &remote);
	void addRemoteURL(const Remote &remote);
	void removeRemote(QString const &name);
	QStringList getRemotes();

	struct User {
		QString name;
		QString email;
	};
	enum class Source {
		Default,
		Global,
		Local,
	};

	User getUser(Source purpose);
	void setUser(User const&user, bool global);

	bool reset_head1();
	bool reset_hard();
	bool clean_df();
	void push_u(QString const &remote, QString const &branch, AbstractPtyProcess *pty);
	QString objectType(QString const &id);
	bool rm_cached(QString const &file);
	void cherrypick(QString const &name);
	QString getCherryPicking() const;

	QString getMessage(const QString &id);

	struct ReflogItem {
		QString id;
		QString head;
		QString command;
		QString message;
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
	using ReflogItemList = QList<ReflogItem>;

	bool reflog(ReflogItemList *out, int maxcount = 100);
	QByteArray blame(QString const &path);

	enum SignPolicy {
		Unset,
		False,
		True,
	};
	QString signingKey(Source purpose);
	bool setSigningKey(QString const &id, bool global);
	SignPolicy signPolicy(Source source);
	bool setSignPolicy(Source source, SignPolicy policy);
	bool configGpgProgram(QString const &path, bool global);

	struct RemoteInfo {
		QString commit_id;
		QString name;
	};
	QList<RemoteInfo> ls_remote();

	bool stash();
	bool stash_apply();
	bool stash_drop();
};

void parseDiff(std::string const &s, Git::Diff const *info, Git::Diff *out);

#endif // GIT_H
