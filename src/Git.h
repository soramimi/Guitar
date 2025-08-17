#ifndef GIT_H
#define GIT_H

#include "GitBasicSession.h"
#include "GitTypes.h"
#include "common/misc.h"
#include <QDateTime>
#include <zlib.h>


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
	static GitSignatureGrade evaluateSignature(char c)
	{
		switch (c) {
		case 'G':
			return GitSignatureGrade::Good;
		case 'U':
		case 'X':
		case 'Y':
			return GitSignatureGrade::Dubious;
		case 'B':
		case 'R':
			return GitSignatureGrade::Bad;
		case 'E':
			return GitSignatureGrade::Missing;
		case 'N':
		case ' ':
		case 0:
			return GitSignatureGrade::NoSignature;
		}
		return GitSignatureGrade::Unknown;
	}

	static bool isUncommited(GitCommitItem const &item)
	{
		return !item.commit_id.isValid();
	}

private:
	QStringList make_branch_list_(const std::optional<GitResult> &result);
	std::vector<GitFileStatus> status_s_();
	bool commit_(QString const &msg, bool amend, bool sign, AbstractPtyProcess *pty);
	static void parseAheadBehind(QString const &s, GitBranch *b);
	Git();
	void _init(const GitContext &cx);
	QString encodeQuotedText(QString const &str);
	static std::optional<GitCommitItem> parseCommitItem(const QString &line);
public:
	Git(GitContext const &cx, QString const &repodir, QString const &submodpath, QString const &sshkey);
	Git(Git &&r) = delete;
	~Git() = default;

	void clearCommandCache();

	AbstractGitSession::Info &gitinfo()
	{
		return session_->gitinfo();
	}
	AbstractGitSession::Info const &gitinfo() const
	{
		return session_->gitinfo();
	}

	AbstractGitSession::Info2 &gitinfo2()
	{
		return session_->gitinfo2();
	}
	AbstractGitSession::Info2 const &gitinfo2() const
	{
		return session_->gitinfo2();
	}

	QByteArray toQByteArray(const std::optional<GitResult> &var) const;
	bool isValidGitCommand() const
	{
		return session_->is_connected();
	}
	std::string resultStdString(const std::optional<GitResult> &var) const;
	QString resultQString(const std::optional<GitResult> &var) const;
	std::optional<GitResult> exec_git(QString const &arg, AbstractGitSession::Option const &opt)
	{
		return session_->exec_git(arg, opt);
	}
	std::optional<GitResult> git(QString const &arg)
	{
		return exec_git(arg, {});
	}
	std::optional<GitResult> git_nolog(QString const &arg, AbstractPtyProcess *pty)
	{
		AbstractGitSession::Option opt;
		opt.pty = pty;
		opt.log = false;
		return exec_git(arg, opt);
	}
	std::optional<GitResult> git_nochdir(QString const &arg, AbstractPtyProcess *pty)
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

	GitCommitItemList log_all(GitHash const &id, int maxcount);
	GitCommitItemList log_file(QString const &path, int maxcount);
	QStringList rev_list_all(GitHash const &id, int maxcount);

	std::optional<GitCommitItem> log_signature(GitHash const &id);
	GitCommitItemList log(int maxcount);
	std::optional<GitCommitItem> queryCommitItem(const GitHash &id);

	static GitCloneData preclone(QString const &url, QString const &path);
	bool clone(GitCloneData const &data, AbstractPtyProcess *pty);

	std::vector<GitFileStatus> status_s();
	std::optional<QByteArray> cat_file(GitHash const &id);
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

	QList<GitBranch> branches();

	QString diff(QString const &old_id, QString const &new_id);
	QString diff_file(QString const &old_path, QString const &new_path);

	std::string diff_head(std::function<bool (std::string const &, std::string const &)> fn_accept = nullptr);

	QList<GitDiffRaw> diff_raw(GitHash const &old_id, GitHash const &new_id);

	QString status();
	bool commit(QString const &text, bool sign, AbstractPtyProcess *pty);
	bool commit_amend_m(QString const &text, bool sign, AbstractPtyProcess *pty);
	bool revert(const GitHash &id);
	bool push_tags(AbstractPtyProcess *pty = nullptr);
	void remote_v(std::vector<GitRemote> *out);
	void createBranch(QString const &name);
	void checkoutBranch(QString const &name);
	void mergeBranch(QString const &name, GitMergeFastForward ff, bool squash);
	bool deleteBranch(QString const &name);

	bool checkout(QString const &branch_name, QString const &id = {});
	bool checkout_detach(QString const &id);

	void rebaseBranch(QString const &name);
	void rebase_abort();

	bool isValidWorkingCopy(QString const &dir) const;
	QString diff_to_file(QString const &old_id, QString const &path);
	QString errorMessage(const std::optional<GitResult> &var) const;

	GitHash rev_parse(QString const &name);
	QList<GitTag> tags();
	bool tag(QString const &name, GitHash const &id = {});
	bool delete_tag(QString const &name, bool remote);
	void setRemoteURL(const GitRemote &remote);
	void addRemoteURL(const GitRemote &remote);
	void removeRemote(QString const &name);
	QStringList getRemotes();

	GitUser getUser(GitSource purpose);
	void setUser(GitUser const&user, bool global);

	bool reset_head1();
	bool reset_hard();
	bool clean_df();
	bool push_u(bool set_upstream, QString const &remote, QString const &branch, bool force, AbstractPtyProcess *pty);
	QString objectType(const GitHash &id);
	bool rm_cached(QString const &file);
	void cherrypick(QString const &name);
	QString getCherryPicking() const;

	QString getMessage(const QString &id);

	using ReflogItemList = QList<GitReflogItem>;

	bool reflog(ReflogItemList *out, int maxcount = 100);
	QByteArray blame(QString const &path);

	QString signingKey(GitSource purpose);
	bool setSigningKey(QString const &id, bool global);
	GitSignPolicy signPolicy(GitSource source);
	bool setSignPolicy(GitSource source, GitSignPolicy policy);
	bool configGpgProgram(QString const &path, bool global);

	struct RemoteInfo {
		QString commit_id;
		QString name;
	};
	QList<RemoteInfo> ls_remote();

	bool stash();
	bool stash_apply();
	bool stash_drop();

	QList<GitSubmoduleItem> submodules();
	bool submodule_add(const GitCloneData &data, bool force, AbstractPtyProcess *pty);
	bool submodule_update(const GitSubmoduleUpdateData &data, AbstractPtyProcess *pty);
	static std::optional<GitCommitItem> parseCommit(QByteArray const &ba);
	QString queryEntireCommitMessage(const GitHash &id);

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
	GitHash id;
};
using NamedCommitList = QList<NamedCommitItem>;

void parseDiff(const std::string_view &s, GitDiff const *info, GitDiff *out);

void parseGitSubModules(QByteArray const &ba, QList<GitSubmoduleItem> *out);

#endif // GIT_H
