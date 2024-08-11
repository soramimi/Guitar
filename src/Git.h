#ifndef GIT_H
#define GIT_H

#include "AbstractProcess.h"
#include "common/misc.h"
#include <QDateTime>
#include <QDebug>
#include <QMutex>
#include <QObject>
#include <functional>
#include <memory>
#include <optional>
#include <functional>

#define SINGLE_THREAD 0

#define GIT_ID_LENGTH (40)

#define PATH_PREFIX "*"

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

class Git;
using GitPtr = std::shared_ptr<Git>;

class Git : QObject {
public:
	class CommitID {
	private:
		bool valid = false;
		uint8_t id[GIT_ID_LENGTH / 2];
	public:
		CommitID();
		explicit CommitID(QString const &qid);
		explicit CommitID(char const *id);
		void assign(const QString &qid);
		QString toQString(int maxlen = -1) const;
		bool isValid() const;
		int compare(CommitID const &other) const
		{
			if (!valid && other.valid) return -1;
			if (valid && !other.valid) return 1;
			if (!valid && !other.valid) return 0;
			return memcmp(id, other.id, sizeof(id));
		}
		operator bool () const
		{
			return isValid();
		}
	};
	class Context {
	public:
		QString git_command;
		QString ssh_command;
		QString ssh_private_key;
	};

	struct Object {
		enum class Type { // 値は固定。packフォーマットで決まってる
			NONE = -1,
			UNKNOWN = 0,
			COMMIT = 1,
			TREE = 2,
			BLOB = 3,
			TAG = 4,
			UNDEFINED = 5,
			OFS_DELTA = 6,
			REF_DELTA = 7,
		};
		Type type = Type::NONE;
		QByteArray content;
	};

	struct SubmoduleItem {
		QString name;
		Git::CommitID id;
		QString path;
		QString refs;
		QString url;
		operator bool () const
		{
			return isValidID(id) && !path.isEmpty();
		}
	};

	enum class SignatureGrade {
		NoSignature,
		Unknown,
		Good,
		Dubious,
		Missing,
		Bad,
	};

	struct CommitItem {
		CommitID commit_id;
		QList<CommitID> parent_ids;
		QString author;
		QString email;
		QString message;
		QDateTime commit_date;
		std::vector<TreeLine> parent_lines;
		QString gpgsig;
		struct {
			QString text;
			char verify = 0; // git log format:%G?
			std::vector<uint8_t> key_fingerprint;
			QString trust;
		} sign;
		bool has_child = false;
		int marker_depth = -1;
		bool resolved =  false;
		bool strange_date = false;
		void setParents(QStringList const &list);
		operator bool () const
		{
			return commit_id;
		}
	};

	class CommitItemList {
	public:
		std::vector<CommitItem> list;
		std::map<CommitID, size_t> index;
		size_t size() const
		{
			return list.size();
		}
		CommitItem &at(size_t i)
		{
			return list.at(i);
		}
		CommitItem const &at(size_t i) const
		{
			return list.at(i);
		}
		CommitItem &operator [] (size_t i)
		{
			return at(i);
		}
		CommitItem const &operator [] (size_t i) const
		{
			return at(i);
		}
		void clear()
		{
			list.clear();
		}
		bool empty() const
		{
			return list.empty();
		}
		void updateIndex()
		{
			index.clear();
			for (size_t i = 0; i < list.size(); i++) {
				index[list[i].commit_id] = i;
			}
		}
		CommitItem *find(CommitID const &id)
		{
			auto it = index.find(id);
			if (it != index.end()) {
				return &list[it->second];
			}
			return nullptr;
		}
		CommitItem const *find(CommitID const &id) const
		{
			return const_cast<CommitItemList *>(this)->find(id);
		}
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
			QString a_id_or_path; // コミットIDまたはファイルパス。パスのときは PATH_PREFIX（'*'）で始まる
			QString b_id_or_path;
		} blob;
		QList<Hunk> hunks;
		struct SubmoduleDetail {
			Git::SubmoduleItem item;
			Git::CommitItem commit;
		} a_submodule, b_submodule;
		Diff() = default;
		Diff(QString const &id, QString const &path, QString const &mode)
		{
			makeForSingleFile(this, QString(GIT_ID_LENGTH, '0'), id, path, mode);
		}
		bool isSubmodule() const
		{
			return mode == "160000";
		}
	private:
		static void makeForSingleFile(Git::Diff *diff, QString const &id_a, QString const &id_b, QString const &path, QString const &mode);
	};

	static SignatureGrade evaluateSignature(char c)
	{
		switch (c) {
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

	static bool isUncommited(CommitItem const &item)
	{
		return !item.commit_id.isValid();
	}

	struct Branch {
		QString name;
		Git::CommitID id;
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
			return id.isValid() && !name.isEmpty();
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
		Git::CommitID id;
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
	QStringList make_branch_list_() const;
	QByteArray cat_file_(const CommitID &id);
	FileStatusList status_s_();
	bool commit_(QString const &msg, bool amend, bool sign, AbstractPtyProcess *pty);
//	bool push_(bool tags, AbstractPtyProcess *pty);
	static void parseAheadBehind(QString const &s, Branch *b);
	Git();
	QString encodeQuotedText(QString const &str);
	static std::optional<CommitItem> parseCommitItem(const QString &line);
public:
	Git(Context const &cx, QString const &repodir, QString const &submodpath, QString const &sshkey);
	Git(Git &&r) = delete;
	 ~Git() override;

	// using callback_t = bool (*)(void *, const char *, int);

	void setLogCallback(std::function<bool (void *, const char *, int)> func, void *cookie);

	QByteArray toQByteArray() const;
	void setGitCommand(QString const &gitcmd, const QString &sshcmd = {});
	QString gitCommand() const;
	void clearResult();
	std::string resultStdString() const;
	QString resultQString() const;
	bool chdirexec(std::function<bool ()> const &fn) const;
	struct Option {
		bool chdir = true;
		bool log = true;
		bool errout = false;
		AbstractPtyProcess *pty = nullptr;
		QString prefix;
	};
	bool git(QString const &arg, Option const &opt, bool debug_ = false);
	bool git(QString const &arg)
	{
		return git(arg, {});
	}
	bool git_nolog(QString const &arg, AbstractPtyProcess *pty)
	{
		Option opt;
		opt.pty = pty;
		opt.log = false;
		return git(arg, opt);
	}
	bool git_nochdir(QString const &arg, AbstractPtyProcess *pty)
	{
		Option opt;
		opt.pty = pty;
		opt.chdir = false;
		return git(arg, opt);
	}

	void setWorkingRepositoryDir(QString const &repo, const QString &submodpath, const QString &sshkey);
	QString workingDir() const;
	QString const &sshKey() const;
	void setSshKey(const QString &sshkey) const;

	QString getCurrentBranchName();
	bool isValidWorkingCopy() const;
	QString version();
	bool init();
	QStringList getUntrackedFiles();
	CommitItemList log_all(CommitID const &id, int maxcount);
	std::optional<CommitItem> log_signature(CommitID const &id);
	CommitItemList log(int maxcount);
	std::optional<CommitItem> queryCommitItem(const CommitID &id);

	struct CloneData {
		QString url;
		QString basedir;
		QString subdir;
	};
	static CloneData preclone(QString const &url, QString const &path);
	bool clone(CloneData const &data, AbstractPtyProcess *pty);

	FileStatusList status_s();
	std::optional<QByteArray> cat_file(const CommitID &id);
	void resetFile(QString const &path);
	void resetAllFiles();

	void removeFile(QString const &path);

	void add_A();
	bool unstage_all();

	void stage(QString const &path);
	bool stage(QStringList const &paths, AbstractPtyProcess *pty);
	void unstage(QString const &path);
	void unstage(QStringList const &paths);
	bool pull(AbstractPtyProcess *pty = nullptr);

	bool fetch(AbstractPtyProcess *pty = nullptr, bool prune = false);
	bool fetch_tags_f(AbstractPtyProcess *pty);

	QList<Branch> branches();

	int getProcessExitCode() const;

	QString diff(QString const &old_id, QString const &new_id);
	QString diff_file(QString const &old_path, QString const &new_path);

	QString diff_head(std::function<bool (QString const &name, QString const &mime)> fn_accept = nullptr);

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
		QString url_fetch;
		QString url_push;
		QString ssh_key;
		bool operator < (Remote const &r) const
		{
			return name < r.name;
		}
		QString const &url() const
		{
			return url_fetch;
		}
		void set_url(QString const &url)
		{
			url_fetch = url;
			url_push = url;
		}
	};

	QList<DiffRaw> diff_raw(CommitID const &old_id, CommitID const &new_id);

	static bool isValidID(QString const &id);
	static bool isValidID(CommitID const &id)
	{
		return id.isValid();
	}

	QString status();
	bool commit(QString const &text, bool sign, AbstractPtyProcess *pty);
	bool commit_amend_m(QString const &text, bool sign, AbstractPtyProcess *pty);
	bool revert(const CommitID &id);
	bool push_tags(AbstractPtyProcess *pty = nullptr);
	void remote_v(std::vector<Remote> *out);
	void createBranch(QString const &name);
	void checkoutBranch(QString const &name);
	void mergeBranch(QString const &name, MergeFastForward ff, bool squash);

	void rebaseBranch(QString const &name);
	void rebase_abort();

	static bool isValidWorkingCopy(QString const &dir);
	QString diff_to_file(QString const &old_id, QString const &path);
	QString errorMessage() const;

	GitPtr dup() const;
	CommitID rev_parse(QString const &name);
	QList<Tag> tags();
	QList<Tag> tags2();
	bool tag(QString const &name, CommitID const &id = {});
	bool delete_tag(QString const &name, bool remote);
	void setRemoteURL(const Remote &remote);
	void addRemoteURL(const Remote &remote);
	void removeRemote(QString const &name);
	QStringList getRemotes();

	struct User {
		QString name;
		QString email;
		operator bool () const
		{
			return misc::isValidMailAddress(email);
		}
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
	bool push_u(bool set_upstream, QString const &remote, QString const &branch, bool force, AbstractPtyProcess *pty);
	QString objectType(const CommitID &id);
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

	struct SubmoduleUpdateData {
		bool init = true;
		bool recursive = true;
	};


	QList<SubmoduleItem> submodules();
	bool submodule_add(const CloneData &data, bool force, AbstractPtyProcess *pty);
	bool submodule_update(const SubmoduleUpdateData &data, AbstractPtyProcess *pty);
	static CommitItem parseCommit(const QByteArray &ba);
	QString queryEntireCommitMessage(const CommitID &id);
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
	Git::CommitID id;
};
using NamedCommitList = QList<NamedCommitItem>;

void parseDiff(std::string const &s, Git::Diff const *info, Git::Diff *out);

void parseGitSubModules(QByteArray const &ba, QList<Git::SubmoduleItem> *out);

static inline bool operator == (Git::CommitID const &l, Git::CommitID const &r)
{
	return l.compare(r) == 0;
}

static inline bool operator < (Git::CommitID const &l, Git::CommitID const &r)
{
	return l.compare(r) < 0;
}

#endif // GIT_H
