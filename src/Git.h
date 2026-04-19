#ifndef GIT_H
#define GIT_H

#include "AbstractGitSession.h"
#include "GitTypes.h"
#include <QDateTime>
#include <zlib.h>

class GitContext {
public:
	std::string git_command;
	std::string ssh_command;

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
	std::vector<std::string> make_branch_list_(const std::optional<GitResult> &result);
	std::vector<GitFileStatus> status_s_();
	bool commit_(const std::string &msg, bool amend, bool sign, AbstractPtyProcess *pty);
	static void parseAheadBehind(QString const &s, GitBranch *b);
	Git();
	void _init(const GitContext &cx);
	QString encodeQuotedText(QString const &str);
	std::string encodeQuotedText(std::string const &str);
	static std::optional<GitCommitItem> parseCommitItem(const std::string &line);
	std::string submoduleURL(std::string const &path);
public:
	Git(GitContext const &cx, std::string const &repodir, std::string const &submodpath, std::string const &sshkey);
	Git(Git &&r) = delete;
	~Git() = default;

	void clearCommandCache();
	void clearObjectCache();

	AbstractGitSession::Info &gitinfo()
	{
		return session_->gitinfo();
	}
	AbstractGitSession::Info const &gitinfo() const
	{
		return session_->gitinfo();
	}

		std::vector<char> toByteArray(const std::optional<GitResult> &var) const;
	bool isValidGitCommand() const
	{
		return session_->is_connected();
	}
	std::string resultStdString(const std::optional<GitResult> &var) const;
	QString resultQString(const std::optional<GitResult> &var) const;
	std::optional<GitResult> exec_git(std::string const &arg, AbstractGitSession::Option const &opt)
	{
		return session_->exec_git(arg, opt);
	}
	std::optional<GitResult> git(std::string const &arg)
	{
		return exec_git(arg, {});
	}
	std::optional<GitResult> git_nolog(std::string const &arg, AbstractPtyProcess *pty)
	{
		AbstractGitSession::Option opt;
		opt.pty = pty;
		opt.log = false;
		return exec_git(arg, opt);
	}
	std::optional<GitResult> git_nochdir(std::string const &arg, AbstractPtyProcess *pty)
	{
		AbstractGitSession::Option opt;
		opt.pty = pty;
		opt.chdir = false;
		return exec_git(arg, opt);
	}
	bool remove(std::string const &path)
	{
		return session_->remove(path);
	}

	void setWorkingRepositoryDir(std::string const &repo, std::string const &sshkey);
	void setSubmodulePath(std::string const &submodpath);

	std::string workingDir() const
	{
		return session_->workingDir();
	}
	const std::string &sshKey() const;
	void setSshKey(std::string const &sshkey);

	std::string getCurrentBranchName();
	bool isValidWorkingCopy() const;
	std::string version();
	bool init();
	std::vector<std::string> getUntrackedFiles();

	GitCommitItemList log_file(const std::string &path, int maxcount);
	GitCommitItemList log_all(GitHash const &id, int maxcount);
	std::vector<GitHash> rev_list_all(GitHash const &id, int maxcount);

	std::optional<GitCommitItem> log_signature(GitHash const &id);
	GitCommitItemList log(int maxcount);
	std::optional<GitCommitItem> queryCommitItem(const GitHash &id);

	static GitCloneData preclone(QString const &url, QString const &path);
	bool clone(GitCloneData const &data, AbstractPtyProcess *pty);

	std::vector<GitFileStatus> status_s();
	std::optional<std::vector<char> > cat_file(GitHash const &id);
	void resetFile(const std::string &path);
	void resetAllFiles();
	
	void rm(const std::string &path, bool rm_real_file);

	void add_A();
	bool unstage_all();

	void stage(const std::string &path);
	bool stage(std::vector<std::string> const &paths, AbstractPtyProcess *pty);
	void unstage(const std::string &path);
	void unstage(std::vector<std::string> const &paths);
	bool pull(AbstractPtyProcess *pty = nullptr);

	bool fetch(AbstractPtyProcess *pty = nullptr, bool prune = false);

	QList<GitBranch> branches();

	std::string diff(const std::string &old_id, const std::string &new_id);
	std::string diff_file(const std::string &old_path, const std::string &new_path);

	std::vector<std::string> diff_name_only_head();
	std::string diff_full_index_head_file(const std::string &file);

	QList<GitDiffRaw> diff_raw(GitHash const &old_id, GitHash const &new_id);

	std::string status();
	bool commit(const std::string &text, bool sign, AbstractPtyProcess *pty);
	bool commit_amend_m(const std::string &text, bool sign, AbstractPtyProcess *pty);
	bool revert(const GitHash &id);
	bool push_tags(AbstractPtyProcess *pty = nullptr);
	void remote_v(std::vector<GitRemote> *out);
	void createBranch(const std::string &name);
	void checkoutBranch(std::string const &name);
	void mergeBranch(const std::string &name, GitMergeFastForward ff, bool squash);
	bool deleteBranch(std::string const &name);

	bool checkout(const std::string &branch_name, const std::string &id = {});
	bool checkout_detach(std::string const &id);

	void rebaseBranch(const std::string &name);
	void rebase_abort();

	bool isValidWorkingCopy(const std::string &dir) const;
	std::string diff_to_file(const std::string &old_id, const std::string &path);
	std::string errorMessage(const std::optional<GitResult> &var) const;

	GitHash rev_parse(std::string const &name);
	QList<GitTag> tags();
	bool tag(const std::string &name, GitHash const &id = {});
	bool delete_tag(const std::string &name, bool remote);
	void setRemoteURL(const GitRemote &remote);
	void addRemoteURL(const GitRemote &remote);
	void removeRemote(const std::string &name);
	std::vector<std::string> getRemotes();

	GitUser getUser(GitSource purpose);
	void setUser(GitUser const&user, bool global);

	bool reset_head1();
	bool reset_hard();
	bool clean_df();
	bool push_u(bool set_upstream, std::string const &remote, std::string const &branch, bool force, AbstractPtyProcess *pty);
	std::string objectType(const GitHash &id);
	bool rm_cached(const std::string &file);
	void cherrypick(std::string const &name);
	std::string getCherryPicking() const;

	std::string getMessage(const std::string &id);

	using ReflogItemList = QList<GitReflogItem>;

	bool reflog(ReflogItemList *out, int maxcount = 100);
	std::vector<char> blame(const std::string &path);

	std::string signingKey(GitSource purpose);
	bool setSigningKey(const std::string &id, bool global);
	GitSignPolicy signPolicy(GitSource source);
	bool setSignPolicy(GitSource source, GitSignPolicy policy);
	bool configGpgProgram(const std::string &path, bool global);

	struct RemoteInfo {
		std::string commit_id;
		std::string name;
	};
	QList<RemoteInfo> ls_remote();

	bool stash();
	bool stash_apply();
	bool stash_drop();

	QList<GitSubmoduleItem> submodules();
	bool submodule_add(const GitCloneData &data, bool force, AbstractPtyProcess *pty);
	bool submodule_update(const GitSubmoduleUpdateData &data, AbstractPtyProcess *pty);
	static std::optional<GitCommitItem> parseCommit(const std::vector<char> &ba);
	std::string queryEntireCommitMessage(const GitHash &id);

	std::string getDefaultBranch();
	void setDefaultBranch(const std::string &branchname);
	void unsetDefaultBranch();
	QDateTime repositoryLastModifiedTime();

	std::optional<std::vector<GitFileItem>> ls(std::string const &path);
	std::optional<std::vector<char>> readfile(std::string const &path);
};

struct NamedCommitItem {
	enum class Type {
		None,
		BranchLocal,
		BranchRemote,
		Tag,
	};
	Type type = Type::None;
	std::string remote;
	std::string name;
	GitHash id;
};
using NamedCommitList = QList<NamedCommitItem>;

void parseDiff(const std::string_view &s, GitDiff const *info, GitDiff *out);

void parseGitSubModules(QByteArray const &ba, QList<GitSubmoduleItem> *out);

#endif // GIT_H
