#ifndef GIT_H
#define GIT_H

#include "GitBasicSession.h"
#include "common/misc.h"
#include <QDateTime>
#include "GitTypes.h"

#define GIT_ID_LENGTH (40)
#define PATH_PREFIX "*"

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

class GitContext {
public:
	QString git_command;
	QString ssh_command;

	std::shared_ptr<AbstractGitSession> connect() const;
};

class Git {
	friend class GitRunner;
private:
	std::shared_ptr<AbstractGitSession> session_;
public:
	class Hash {
	private:
		bool valid_ = false;
		uint8_t id_[GIT_ID_LENGTH / 2];
		template <typename VIEW> void _assign(VIEW const &id);
	public:
		Hash() {}
		explicit Hash(std::string_view const &id);
		explicit Hash(QString const &id);
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
		Hash tree;
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

	static QString trimPath(QString const &s);

private:
	// struct Private;
	// Private *m;
	QStringList make_branch_list_();
	FileStatusList status_s_();
	bool commit_(QString const &msg, bool amend, bool sign, AbstractPtyProcess *pty);
	static void parseAheadBehind(QString const &s, Branch *b);
	Git();
	void _init(const GitContext &cx);
	QString encodeQuotedText(QString const &str);
	static std::optional<CommitItem> parseCommitItem(const QString &line);
public:
	Git(GitContext const &cx, QString const &repodir, QString const &submodpath, QString const &sshkey);
	Git(Git &&r) = delete;
	~Git() = default;

	AbstractGitSession::Info &gitinfo()
	{
		return session_->gitinfo();
	}
	AbstractGitSession::Info const &gitinfo() const
	{
		return session_->gitinfo();
	}

	AbstractGitSession::Var const &var() const
	{
		return session_->var();
	}

	void setCommandCache(GitCommandCache const &cc)
	{
		session_->set_command_cache(cc);
	}

	std::vector<char> const &toCharVector() const;
	QByteArray toQByteArray() const;
	bool isValidGitCommand() const
	{
		return session_->is_connected();
	}
	std::string_view resultStdString() const;
	QString resultQString() const;
	bool exec_git(QString const &arg, AbstractGitSession::Option const &opt)
	{
		return session_->exec_git(arg, opt);
	}
	bool git(QString const &arg)
	{
		return exec_git(arg, {});
	}
	bool git_nolog(QString const &arg, AbstractPtyProcess *pty)
	{
		AbstractGitSession::Option opt;
		opt.pty = pty;
		opt.log = false;
		return exec_git(arg, opt);
	}
	bool git_nochdir(QString const &arg, AbstractPtyProcess *pty)
	{
		AbstractGitSession::Option opt;
		opt.pty = pty;
		opt.chdir = false;
		return exec_git(arg, opt);
	}
	bool remove(QString const &path)
	{
		return session_->remove(path);
	}

	void setWorkingRepositoryDir(QString const &repo, const QString &sshkey);
	void setSubmodulePath(const QString &submodpath);

	QString workingDir() const
	{
		return session_->workingDir();
	}
	QString const &sshKey() const;
	void setSshKey(const QString &sshkey);

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
	
	void rm(QString const &path, bool rm_real_file);

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

	std::string diff_head(std::function<bool (std::string const &, std::string const &)> fn_accept = nullptr);

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

	bool isValidWorkingCopy(QString const &dir) const;
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

	std::optional<std::vector<GitFileItem>> ls(const QString &path);
	std::optional<std::vector<char>> readfile(const QString &path);
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

using GitPtr = std::shared_ptr<Git>;

// GitRunner

class GitRunner {
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

	static std::optional<Git::CommitItem> parseCommit(QByteArray const &ba)
	{
		return Git::parseCommit(ba);
	}

	bool isValidWorkingCopy(QString const &dir) const
	{
		return git && git->isValidWorkingCopy(dir);
	}

	bool isValidWorkingCopy() const
	{
		return git && git->isValidWorkingCopy();
	}

	void setWorkingRepositoryDir(QString const &repo, const QString &sshkey)
	{
		Q_ASSERT(git);
		git->setWorkingRepositoryDir(repo, sshkey);
	}
	void setSubmodulePath(const QString &submodpath)
	{
		Q_ASSERT(git);
		git->setSubmodulePath(submodpath);
	}
	QString workingDir() const
	{
		Q_ASSERT(git);
		return git->workingDir();
	}
	QString const &sshKey() const
	{
		Q_ASSERT(git);
		return git->sshKey();
	}
	void setSshKey(const QString &sshkey) const
	{
		Q_ASSERT(git);
		git->setSshKey(sshkey);
	}

	QString getMessage(const QString &id)
	{
		Q_ASSERT(git);
		return git->getMessage(id);
	}
	QString errorMessage() const
	{
		Q_ASSERT(git);
		return git->errorMessage();
	}

	// bool pushd(std::function<bool ()> const fn)
	// {
	// 	Q_ASSERT(git);
	// 	return git->pushd(fn);
	// }

	bool remove(QString const &path)
	{
		Q_ASSERT(git);
		return git->remove(path);
	}

	Git::Hash rev_parse(QString const &name)
	{
		Q_ASSERT(git);
		return git->rev_parse(name);
	}
	void setRemoteURL(const Git::Remote &remote)
	{
		Q_ASSERT(git);
		git->setRemoteURL(remote);
	}
	void addRemoteURL(const Git::Remote &remote)
	{
		Q_ASSERT(git);
		git->addRemoteURL(remote);
	}
	void removeRemote(QString const &name)
	{
		Q_ASSERT(git);
		git->removeRemote(name);
	}
	QStringList getRemotes()
	{
		Q_ASSERT(git);
		return git->getRemotes();
	}

	QString version()
	{
		Q_ASSERT(git);
		return git->version();
	}

	bool init()
	{
		Q_ASSERT(git);
		return git->init();
	}

	QList<Git::Tag> tags()
	{
		Q_ASSERT(git);
		return git->tags();
	}
	bool tag(QString const &name, Git::Hash const &id = {})
	{
		Q_ASSERT(git);
		return git->tag(name, id);
	}
	bool delete_tag(QString const &name, bool remote)
	{
		Q_ASSERT(git);
		return git->delete_tag(name, remote);
	}

	void resetFile(QString const &path)
	{
		Q_ASSERT(git);
		git->resetFile(path);
	}
	void resetAllFiles()
	{
		Q_ASSERT(git);
		git->resetAllFiles();
	}

	void removeFile(QString const &path, bool rm_real_file)
	{
		Q_ASSERT(git);
		git->rm(path, rm_real_file);
	}

	Git::User getUser(Git::Source purpose)
	{
		Q_ASSERT(git);
		return git->getUser(purpose);
	}
	void setUser(Git::User const&user, bool global)
	{
		Q_ASSERT(git);
		git->setUser(user, global);
	}
	QString getDefaultBranch()
	{
		Q_ASSERT(git);
		return git->getDefaultBranch();
	}
	void setDefaultBranch(QString const &branchname)
	{
		Q_ASSERT(git);
		git->setDefaultBranch(branchname);
	}
	void unsetDefaultBranch()
	{
		Q_ASSERT(git);
		git->unsetDefaultBranch();
	}
	QDateTime repositoryLastModifiedTime()
	{
		Q_ASSERT(git);
		return git->repositoryLastModifiedTime();
	}
	QString status()
	{
		Q_ASSERT(git);
		return git->status();
	}
	bool commit(QString const &text, bool sign, AbstractPtyProcess *pty)
	{
		Q_ASSERT(git);
		return git->commit(text, sign, pty);
	}
	bool commit_amend_m(QString const &text, bool sign, AbstractPtyProcess *pty)
	{
		Q_ASSERT(git);
		return git->commit_amend_m(text, sign, pty);
	}
	bool revert(const Git::Hash &id)
	{
		Q_ASSERT(git);
		return git->revert(id);
	}
	bool push_tags(AbstractPtyProcess *pty = nullptr)
	{
		Q_ASSERT(git);
		return git->push_tags(pty);
	}
	void remote_v(std::vector<Git::Remote> *out)
	{
		Q_ASSERT(git);
		git->remote_v(out);
	}
	void createBranch(QString const &name)
	{
		Q_ASSERT(git);
		git->createBranch(name);
	}
	void checkoutBranch(QString const &name)
	{
		Q_ASSERT(git);
		git->checkoutBranch(name);
	}
	void mergeBranch(QString const &name, Git::MergeFastForward ff, bool squash)
	{
		Q_ASSERT(git);
		git->mergeBranch(name, ff, squash);
	}
	bool deleteBranch(QString const &name)
	{
		Q_ASSERT(git);
		return git->deleteBranch(name);
	}

	bool checkout(QString const &branch_name, QString const &id = {})
	{
		Q_ASSERT(git);
		return git->checkout(branch_name, id);
	}
	bool checkout_detach(QString const &id)
	{
		Q_ASSERT(git);
		return git->checkout_detach(id);
	}

	void rebaseBranch(QString const &name)
	{
		Q_ASSERT(git);
		git->rebaseBranch(name);
	}
	void rebase_abort()
	{
		Q_ASSERT(git);
		git->rebase_abort();
	}

	Git::CommitItemList log_all(Git::Hash const &id, int maxcount)
	{
		Q_ASSERT(git);
		return git->log_all(id, maxcount);
	}
	Git::CommitItemList log_file(QString const &path, int maxcount)
	{
		Q_ASSERT(git);
		return git->log_file(path, maxcount);
	}
	QStringList rev_list_all(Git::Hash const &id, int maxcount)
	{
		Q_ASSERT(git);
		return git->rev_list_all(id, maxcount);
	}

	std::optional<Git::CommitItem> log_signature(Git::Hash const &id)
	{
		Q_ASSERT(git);
		return git->log_signature(id);
	}
	Git::CommitItemList log(int maxcount)
	{
		Q_ASSERT(git);
		return git->log(maxcount);
	}
	std::optional<Git::CommitItem> queryCommitItem(const Git::Hash &id)
	{
		Q_ASSERT(git);
		return git->queryCommitItem(id);
	}

	bool stash()
	{
		Q_ASSERT(git);
		return git->stash();
	}
	bool stash_apply()
	{
		Q_ASSERT(git);
		return git->stash_apply();
	}
	bool stash_drop()
	{
		Q_ASSERT(git);
		return git->stash_drop();
	}

	QList<Git::SubmoduleItem> submodules()
	{
		Q_ASSERT(git);
		return git->submodules();
	}
	bool submodule_add(const Git::CloneData &data, bool force, AbstractPtyProcess *pty)
	{
		Q_ASSERT(git);
		return git->submodule_add(data, force, pty);
	}
	bool submodule_update(const Git::SubmoduleUpdateData &data, AbstractPtyProcess *pty)
	{
		Q_ASSERT(git);
		return git->submodule_update(data, pty);
	}
	QString queryEntireCommitMessage(const Git::Hash &id)
	{
		Q_ASSERT(git);
		return git->queryEntireCommitMessage(id);
	}

	QList<Git::DiffRaw> diff_raw(Git::Hash const &old_id, Git::Hash const &new_id)
	{
		Q_ASSERT(git);
		return git->diff_raw(old_id, new_id);
	}
	std::string diff_head(std::function<bool (std::string const &name, std::string const &mime)> fn_accept = nullptr)
	{
		Q_ASSERT(git);
		return git->diff_head(fn_accept);
	}
	QString diff(QString const &old_id, QString const &new_id)
	{
		Q_ASSERT(git);
		return git->diff(old_id, new_id);
	}
	QString diff_file(QString const &old_path, QString const &new_path)
	{
		Q_ASSERT(git);
		return git->diff_file(old_path, new_path);
	}
	QString diff_to_file(QString const &old_id, QString const &path)
	{
		Q_ASSERT(git);
		return git->diff_to_file(old_id, path);
	}

	Git::FileStatusList status_s()
	{
		Q_ASSERT(git);
		return git->status_s();
	}
	std::optional<QByteArray> cat_file(const Git::Hash &id)
	{
		Q_ASSERT(git);
		return git->cat_file(id);
	}
	bool clone(Git::CloneData const &data, AbstractPtyProcess *pty)
	{
		Q_ASSERT(git);
		return git->clone(data, pty);
	}
	void add_A()
	{
		Q_ASSERT(git);
		git->add_A();
	}
	bool unstage_all()
	{
		Q_ASSERT(git);
		return git->unstage_all();
	}

	void stage(QString const &path)
	{
		Q_ASSERT(git);
		git->stage(path);
	}
	bool stage(QStringList const &paths, AbstractPtyProcess *pty)
	{
		Q_ASSERT(git);
		return git->stage(paths, pty);
	}
	void unstage(QString const &path)
	{
		Q_ASSERT(git);
		git->unstage(path);
	}
	void unstage(QStringList const &paths)
	{
		Q_ASSERT(git);
		git->unstage(paths);
	}
	bool pull(AbstractPtyProcess *pty = nullptr)
	{
		Q_ASSERT(git);
		return git->pull(pty);
	}

	bool fetch(AbstractPtyProcess *pty = nullptr, bool prune = false)
	{
		Q_ASSERT(git);
		return git->fetch(pty, prune);
	}
	bool fetch_tags_f(AbstractPtyProcess *pty)
	{
		Q_ASSERT(git);
		return git->fetch_tags_f(pty);
	}
	bool reset_head1()
	{
		Q_ASSERT(git);
		return git->reset_head1();
	}
	bool reset_hard()
	{
		Q_ASSERT(git);
		return git->reset_hard();
	}
	bool clean_df()
	{
		Q_ASSERT(git);
		return git->clean_df();
	}
	bool push_u(bool set_upstream, QString const &remote, QString const &branch, bool force, AbstractPtyProcess *pty)
	{
		Q_ASSERT(git);
		return git->push_u(set_upstream, remote, branch, force, pty);
	}
	QString objectType(const Git::Hash &id)
	{
		Q_ASSERT(git);
		return git->objectType(id);
	}
	bool rm_cached(QString const &file)
	{
		Q_ASSERT(git);
		return git->rm_cached(file);
	}
	void cherrypick(QString const &name)
	{
		Q_ASSERT(git);
		git->cherrypick(name);
	}
	QString getCherryPicking() const
	{
		Q_ASSERT(git);
		return git->getCherryPicking();
	}
	QList<Git::Branch> branches()
	{
		Q_ASSERT(git);
		return git->branches();
	}

	QString signingKey(Git::Source purpose)
	{
		Q_ASSERT(git);
		return git->signingKey(purpose);
	}
	bool setSigningKey(QString const &id, bool global)
	{
		Q_ASSERT(git);
		return git->setSigningKey(id, global);
	}
	Git::SignPolicy signPolicy(Git::Source source)
	{
		Q_ASSERT(git);
		return git->signPolicy(source);
	}
	bool setSignPolicy(Git::Source source, Git::SignPolicy policy)
	{
		Q_ASSERT(git);
		return git->setSignPolicy(source, policy);
	}
	bool configGpgProgram(QString const &path, bool global)
	{
		Q_ASSERT(git);
		return git->configGpgProgram(path, global);
	}

	bool reflog(Git::ReflogItemList *out, int maxcount = 100)
	{
		Q_ASSERT(git);
		return git->reflog(out, maxcount);
	}
	QByteArray blame(QString const &path)
	{
		Q_ASSERT(git);
		return git->blame(path);
	}

	std::optional<std::vector<GitFileItem>> ls(const QString &path)
	{
		Q_ASSERT(git);
		return git->ls(path);
	}
	std::optional<std::vector<char>> readfile(const QString &path)
	{
		Q_ASSERT(git);
		return git->readfile(path);
	}
};

#endif // GIT_H
