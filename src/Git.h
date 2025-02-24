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

class Git {
	friend class GitRunner;
public:
	class Hash {
	private:
		bool valid_ = false;
		uint8_t id_[GIT_ID_LENGTH / 2];
		template <typename VIEW> void _assign(VIEW const &id);
	public:
		Hash();
		explicit Hash(std::string_view const &id);
		explicit Hash(QString const &qid);
		explicit Hash(char const *id);
		void assign(std::string_view const &qid);
		void assign(const QString &qid);
		QString toQString(int maxlen = -1) const;
		bool isValid() const;
		int compare(Hash const &other) const
		{
			if (!valid_ && other.valid_) return -1;
			if (valid_ && !other.valid_) return 1;
			if (!valid_ && !other.valid_) return 0;
			return memcmp(id_, other.id_, sizeof(id_));
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
		Hash id;
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
		Hash commit_id;
		QString tree;
		QList<Hash> parent_ids;
		QString author;
		QString email;
		QString message;
		QDateTime commit_date;
		std::vector<TreeLine> parent_lines;
		bool has_gpgsig = false;
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
		bool order_fixed = false; // 時差や時計の誤差などの影響により、並び順の調整が行われたとき
		void setParents(QStringList const &list);
		operator bool () const
		{
			return commit_id;
		}
	};

	class CommitItemList {
	public:
		std::vector<CommitItem> list;
		std::map<Hash, size_t> index;
		size_t size() const
		{
			return list.size();
		}
		CommitItem &at(size_t i)
		{
			return list[i];
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
		CommitItem *find(Hash const &id)
		{
			auto it = index.find(id);
			if (it != index.end()) {
				return &list[it->second];
			}
			return nullptr;
		}
		CommitItem const *find(Hash const &id) const
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
			SubmoduleItem item;
			CommitItem commit;
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
		void makeForSingleFile(Diff *diff, QString const &id_a, QString const &id_b, QString const &path, QString const &mode);
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
		Hash id;
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
		Hash id;
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

	class CommandCache {
	public:
		struct Data {
			std::map<QString, std::vector<char>> map;
		};
		std::shared_ptr<Data> d;
		CommandCache(bool make = false)
		{
			if (make) {
				d = std::make_shared<Data>();
			}
		}
		operator bool () const
		{
			return (bool)d;
		}
		void clear()
		{
			if (d) {
				d->map.clear();
			}
		}
		std::vector<char> *find(QString const &key)
		{
			if (!d) return nullptr;
			auto it = d->map.find(key);
			if (it != d->map.end()) {
				return &it->second;
			}
			return nullptr;
		}
		void insert(QString const &key, std::vector<char> const &value)
		{
			if (!d) return;
			d->map[key] = value;
		}
	};

	static QString trimPath(QString const &s);

private:
	struct Private;
	Private *m;
	QStringList make_branch_list_();
	QByteArray cat_file_(Hash const &id);
	FileStatusList status_s_();
	bool commit_(QString const &msg, bool amend, bool sign, AbstractPtyProcess *pty);
	static void parseAheadBehind(QString const &s, Branch *b);
	Git();
	QString encodeQuotedText(QString const &str);
	static std::optional<CommitItem> parseCommitItem(const QString &line);
public:
	Git(Context const &cx, QString const &repodir, QString const &submodpath, QString const &sshkey);
	Git(Git &&r) = delete;
	~Git();

	void setCommandCache(CommandCache const &cc);

	QByteArray toQByteArray() const;
	void setGitCommand(QString const &gitcmd, const QString &sshcmd = {});
	QString gitCommand() const;
	void clearResult();
	std::string resultStdString() const;
	QString resultQString() const;
	bool chdirexec(std::function<bool ()> const &fn);
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

	CommitItemList log_all(Hash const &id, int maxcount);
	CommitItemList log_file(QString const &path, int maxcount);
	QStringList rev_list_all(Hash const &id, int maxcount);

	std::optional<CommitItem> log_signature(Hash const &id);
	CommitItemList log(int maxcount);
	std::optional<CommitItem> queryCommitItem(const Hash &id);

	struct CloneData {
		QString url;
		QString basedir;
		QString subdir;
	};
	static CloneData preclone(QString const &url, QString const &path);
	bool clone(CloneData const &data, AbstractPtyProcess *pty);

	FileStatusList status_s();
	std::optional<QByteArray> cat_file(Hash const &id);
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

	QList<DiffRaw> diff_raw(Hash const &old_id, Hash const &new_id);

	static bool isValidID(QString const &id);
	static bool isValidID(Hash const &id)
	{
		return id.isValid();
	}

	QString status();
	bool commit(QString const &text, bool sign, AbstractPtyProcess *pty);
	bool commit_amend_m(QString const &text, bool sign, AbstractPtyProcess *pty);
	bool revert(const Hash &id);
	bool push_tags(AbstractPtyProcess *pty = nullptr);
	void remote_v(std::vector<Remote> *out);
	void createBranch(QString const &name);
	void checkoutBranch(QString const &name);
	void mergeBranch(QString const &name, MergeFastForward ff, bool squash);
	bool deleteBranch(QString const &name);

	bool checkout(QString const &branch_name, QString const &id = {});
	bool checkout_detach(QString const &id);

	void rebaseBranch(QString const &name);
	void rebase_abort();

	static bool isValidWorkingCopy(QString const &dir);
	QString diff_to_file(QString const &old_id, QString const &path);
	QString errorMessage() const;

	Hash rev_parse(QString const &name);
	QList<Tag> tags();
	bool tag(QString const &name, Hash const &id = {});
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
	QString objectType(const Hash &id);
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
	static std::optional<CommitItem> parseCommit(QByteArray const &ba);
	QString queryEntireCommitMessage(const Hash &id);

	QString getDefaultBranch();
	void setDefaultBranch(QString const &branchname);
	void unsetDefaultBranch();
	QDateTime repositoryLastModifiedTime();
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
	Git::Hash id;
};
using NamedCommitList = QList<NamedCommitItem>;

void parseDiff(const std::string_view &s, Git::Diff const *info, Git::Diff *out);

void parseGitSubModules(QByteArray const &ba, QList<Git::SubmoduleItem> *out);

static inline bool operator == (Git::Hash const &l, Git::Hash const &r)
{
	return l.compare(r) == 0;
}

static inline bool operator < (Git::Hash const &l, Git::Hash const &r)
{
	return l.compare(r) < 0;
}

// GitRunner

class GitRunner {
private:
public:
	GitPtr git;
	GitRunner() = default;
	GitRunner(GitPtr const &git)
		: git(git)
	{
	}
	GitRunner(GitRunner const &that)
		: git(that.git)
	{
	}
	GitRunner(GitRunner &&that)
		: git(std::move(that.git))
	{
	}
	void operator = (GitRunner const &that)
	{
		git = that.git;
	}
	operator bool () const
	{
		return (bool)git;
	}
	GitRunner dup() const;

	void setWorkingRepositoryDir(QString const &repo, const QString &submodpath, const QString &sshkey)
	{
		git->setWorkingRepositoryDir(repo, submodpath, sshkey);
	}
	QString workingDir() const
	{
		return git->workingDir();
	}
	QString const &sshKey() const
	{
		return git->sshKey();
	}
	void setSshKey(const QString &sshkey) const
	{
		git->setSshKey(sshkey);
	}

	QString getMessage(const QString &id)
	{
		return git->getMessage(id);
	}
	QString errorMessage() const
	{
		return git->errorMessage();
	}

	bool chdirexec(std::function<bool ()> const &fn)
	{
		return git->chdirexec(fn);
	}

	Git::Hash rev_parse(QString const &name)
	{
		return git->rev_parse(name);
	}
	void setRemoteURL(const Git::Remote &remote)
	{
		git->setRemoteURL(remote);
	}
	void addRemoteURL(const Git::Remote &remote)
	{
		git->addRemoteURL(remote);
	}
	void removeRemote(QString const &name)
	{
		git->removeRemote(name);
	}
	QStringList getRemotes()
	{
		return git->getRemotes();
	}

	static bool isValidWorkingCopy(QString const &dir)
	{
		return Git::isValidWorkingCopy(dir);
	}

	bool isValidWorkingCopy() const
	{
		return git && git->isValidWorkingCopy();
	}

	QString version()
	{
		return git->version();
	}

	bool init()
	{
		return git->init();
	}

	QList<Git::Tag> tags()
	{
		return git->tags();
	}
	bool tag(QString const &name, Git::Hash const &id = {})
	{
		return git->tag(name, id);
	}
	bool delete_tag(QString const &name, bool remote)
	{
		return git->delete_tag(name, remote);
	}

	void resetFile(QString const &path)
	{
		git->resetFile(path);
	}
	void resetAllFiles()
	{
		git->resetAllFiles();
	}

	void removeFile(QString const &path)
	{
		git->removeFile(path);
	}

	Git::User getUser(Git::Source purpose)
	{
		return git->getUser(purpose);
	}
	void setUser(Git::User const&user, bool global)
	{
		git->setUser(user, global);
	}
	QString getDefaultBranch()
	{
		return git->getDefaultBranch();
	}
	void setDefaultBranch(QString const &branchname)
	{
		git->setDefaultBranch(branchname);
	}
	void unsetDefaultBranch()
	{
		git->unsetDefaultBranch();
	}
	QDateTime repositoryLastModifiedTime()
	{
		return git->repositoryLastModifiedTime();
	}
	QString status()
	{
		return git->status();
	}
	bool commit(QString const &text, bool sign, AbstractPtyProcess *pty)
	{
		return git->commit(text, sign, pty);
	}
	bool commit_amend_m(QString const &text, bool sign, AbstractPtyProcess *pty)
	{
		return git->commit_amend_m(text, sign, pty);
	}
	bool revert(const Git::Hash &id)
	{
		return git->revert(id);
	}
	bool push_tags(AbstractPtyProcess *pty = nullptr)
	{
		return git->push_tags(pty);
	}
	void remote_v(std::vector<Git::Remote> *out)
	{
		git->remote_v(out);
	}
	void createBranch(QString const &name)
	{
		git->createBranch(name);
	}
	void checkoutBranch(QString const &name)
	{
		git->checkoutBranch(name);
	}
	void mergeBranch(QString const &name, Git::MergeFastForward ff, bool squash)
	{
		git->mergeBranch(name, ff, squash);
	}
	bool deleteBranch(QString const &name)
	{
		return git->deleteBranch(name);
	}

	bool checkout(QString const &branch_name, QString const &id = {})
	{
		return git->checkout(branch_name, id);
	}
	bool checkout_detach(QString const &id)
	{
		return git->checkout_detach(id);
	}

	void rebaseBranch(QString const &name)
	{
		git->rebaseBranch(name);
	}
	void rebase_abort()
	{
		git->rebase_abort();
	}

	Git::CommitItemList log_all(Git::Hash const &id, int maxcount)
	{
		return git->log_all(id, maxcount);
	}
	Git::CommitItemList log_file(QString const &path, int maxcount)
	{
		return git->log_file(path, maxcount);
	}
	QStringList rev_list_all(Git::Hash const &id, int maxcount)
	{
		return git->rev_list_all(id, maxcount);
	}

	std::optional<Git::CommitItem> log_signature(Git::Hash const &id)
	{
		return git->log_signature(id);
	}
	Git::CommitItemList log(int maxcount)
	{
		return git->log(maxcount);
	}
	std::optional<Git::CommitItem> queryCommitItem(const Git::Hash &id)
	{
		return git->queryCommitItem(id);
	}

	bool stash()
	{
		return git->stash();
	}
	bool stash_apply()
	{
		return git->stash_apply();
	}
	bool stash_drop()
	{
		return git->stash_drop();
	}

	QList<Git::SubmoduleItem> submodules()
	{
		return git->submodules();
	}
	bool submodule_add(const Git::CloneData &data, bool force, AbstractPtyProcess *pty)
	{
		return git->submodule_add(data, force, pty);
	}
	bool submodule_update(const Git::SubmoduleUpdateData &data, AbstractPtyProcess *pty)
	{
		return git->submodule_update(data, pty);
	}
	static std::optional<Git::CommitItem> parseCommit(QByteArray const &ba)
	{
		return Git::parseCommit(ba);
	}
	QString queryEntireCommitMessage(const Git::Hash &id)
	{
		return git->queryEntireCommitMessage(id);
	}

	QList<Git::DiffRaw> diff_raw(Git::Hash const &old_id, Git::Hash const &new_id)
	{
		return git->diff_raw(old_id, new_id);
	}
	QString diff_head(std::function<bool (QString const &name, QString const &mime)> fn_accept = nullptr)
	{
		return git->diff_head(fn_accept);
	}
	QString diff(QString const &old_id, QString const &new_id)
	{
		return git->diff(old_id, new_id);
	}
	QString diff_file(QString const &old_path, QString const &new_path)
	{
		return git->diff_file(old_path, new_path);
	}
	QString diff_to_file(QString const &old_id, QString const &path)
	{
		return git->diff_to_file(old_id, path);
	}

	Git::FileStatusList status_s()
	{
		return git->status_s();
	}
	std::optional<QByteArray> cat_file(const Git::Hash &id)
	{
		return git->cat_file(id);
	}
	bool clone(Git::CloneData const &data, AbstractPtyProcess *pty)
	{
		return git->clone(data, pty);
	}
	void add_A()
	{
		git->add_A();
	}
	bool unstage_all()
	{
		return git->unstage_all();
	}

	void stage(QString const &path)
	{
		git->stage(path);
	}
	bool stage(QStringList const &paths, AbstractPtyProcess *pty)
	{
		return git->stage(paths, pty);
	}
	void unstage(QString const &path)
	{
		git->unstage(path);
	}
	void unstage(QStringList const &paths)
	{
		git->unstage(paths);
	}
	bool pull(AbstractPtyProcess *pty = nullptr)
	{
		return git->pull(pty);
	}

	bool fetch(AbstractPtyProcess *pty = nullptr, bool prune = false)
	{
		return git->fetch(pty, prune);
	}
	bool fetch_tags_f(AbstractPtyProcess *pty)
	{
		return git->fetch_tags_f(pty);
	}
	bool reset_head1()
	{
		return git->reset_head1();
	}
	bool reset_hard()
	{
		return git->reset_hard();
	}
	bool clean_df()
	{
		return git->clean_df();
	}
	bool push_u(bool set_upstream, QString const &remote, QString const &branch, bool force, AbstractPtyProcess *pty)
	{
		return git->push_u(set_upstream, remote, branch, force, pty);
	}
	QString objectType(const Git::Hash &id)
	{
		return git->objectType(id);
	}
	bool rm_cached(QString const &file)
	{
		return git->rm_cached(file);
	}
	void cherrypick(QString const &name)
	{
		git->cherrypick(name);
	}
	QString getCherryPicking() const
	{
		return git->getCherryPicking();
	}
	QList<Git::Branch> branches()
	{
		return git->branches();
	}

	QString signingKey(Git::Source purpose)
	{
		return git->signingKey(purpose);
	}
	bool setSigningKey(QString const &id, bool global)
	{
		return git->setSigningKey(id, global);
	}
	Git::SignPolicy signPolicy(Git::Source source)
	{
		return git->signPolicy(source);
	}
	bool setSignPolicy(Git::Source source, Git::SignPolicy policy)
	{
		return git->setSignPolicy(source, policy);
	}
	bool configGpgProgram(QString const &path, bool global)
	{
		return git->configGpgProgram(path, global);
	}

	bool reflog(Git::ReflogItemList *out, int maxcount = 100)
	{
		return git->reflog(out, maxcount);
	}
	QByteArray blame(QString const &path)
	{
		return git->blame(path);
	}
};

#endif // GIT_H
