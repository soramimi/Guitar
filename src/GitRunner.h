#ifndef GITRUNNER_H
#define GITRUNNER_H

#include <memory>
#include <optional>
#include <QString>
#include "GitTypes.h"

#define PATH_PREFIX "*"

class Git;
class GitObjectCache;

class GitRunner {
private:
	Git *gitptr()
	{
		Git *p = git.get();
		Q_ASSERT(p);
		return p;
	}
	Git const *gitptr() const
	{
		return const_cast<Git *>(git.get());
	}
public:
	std::shared_ptr<Git> git;
	GitRunner() = default;
	GitRunner(std::shared_ptr<Git> const &git);
	GitRunner(GitRunner const &that);
	GitRunner(GitRunner &&that);
	void operator = (GitRunner const &that);
	operator bool () const;
	GitRunner dup() const;

	GitObjectCache *getObjCache();
	void clearCommandCache();
	void clearObjectCache();

	static std::optional<GitCommitItem> parseCommit(const std::vector<char> &ba);

	bool isValidWorkingCopy(const std::string &dir) const;

	bool isValidWorkingCopy() const;

	void setWorkingRepositoryDir(const std::string &repo, const std::string &sshkey);
	void setSubmodulePath(const std::string &submodpath);
	std::string workingDir() const;
	const std::string &sshKey() const;
	void setSshKey(const std::string &sshkey);

	std::string getMessage(const std::string &id);
	std::string errorMessage(std::optional<GitResult> const &var) const;
	
	bool remove(const std::string &path);

	GitHash rev_parse(const std::string &name, bool use_cache = true);
	void setRemoteURL(const GitRemote &remote);
	void addRemoteURL(const GitRemote &remote);
	void removeRemote(const std::string &name);
        std::vector<std::string> getRemotes();

	std::string version();

	bool init();

        std::vector<GitTag> tags();
	bool tag(const std::string &name, GitHash const &id = {});
	bool delete_tag(const std::string &name, bool remote);

	void resetFile(const std::string &path);
	void resetAllFiles();

	void removeFile(const std::string &path, bool rm_real_file);

	GitUser getUser(GitSource purpose);
	void setUser(GitUser const&user, bool global);
	std::string getDefaultBranch();
	void setDefaultBranch(const std::string &branchname);
	void unsetDefaultBranch();
	QDateTime repositoryLastModifiedTime();
	std::string status();
	bool commit(std::string const &text, bool sign, AbstractPtyProcess *pty);
	bool commit_amend_m(const std::string &text, bool sign, AbstractPtyProcess *pty);
	bool revert(const GitHash &id);
	bool push_tags(AbstractPtyProcess *pty = nullptr);
	void remote_v(std::vector<GitRemote> *out);
	void createBranch(const std::string &name);
	void checkoutBranch(const std::string &name);
	void mergeBranch(const std::string &name, GitMergeFastForward ff, bool squash);
	bool deleteBranch(const std::string &name);

	bool checkout(std::string const &branch_name, std::string const &id = {});
	bool checkout_detach(std::string const &id);

	void rebaseBranch(const std::string &name);
	void rebase_abort();

	GitCommitItemList log_all(GitHash const &id, int maxcount);
	GitCommitItemList log_file(const std::string &path, int maxcount);
	std::vector<GitHash> rev_list_all(GitHash const &id, int maxcount);

	std::optional<GitCommitItem> log_signature(GitHash const &id);
	GitCommitItemList log(int maxcount);
	std::optional<GitCommitItem> queryCommitItem(const GitHash &id);

	bool stash();
	bool stash_apply();
	bool stash_drop();

	QList<GitSubmoduleItem> submodules();
	bool submodule_add(const GitCloneData &data, bool force, AbstractPtyProcess *pty);
	bool submodule_update(const GitSubmoduleUpdateData &data, AbstractPtyProcess *pty);
	std::string queryEntireCommitMessage(const GitHash &id);

        std::vector<GitDiffRaw> diff_raw(GitHash const &old_id, GitHash const &new_id);
	std::string diff(const std::string &old_id, const std::string &new_id);
	std::string diff_file(const std::string &old_path, const std::string &new_path);
	std::string diff_to_file(const std::string &old_id, const std::string &path);
	std::vector<std::string> diff_name_only_head();
	std::string diff_full_index_head_file(const std::string &file);

	std::vector<GitFileStatus> status_s();
	std::optional<std::vector<char> > cat_file_(const GitHash &id);
	GitObject catFile(const GitHash &id, bool use_cache = true);
	bool clone(GitCloneData const &data, AbstractPtyProcess *pty);
	void add_A();
	bool unstage_all();

	void stage(const std::string &path);
	bool stage(const std::vector<std::string> &paths, AbstractPtyProcess *pty);
	void unstage(const std::string &path);
	void unstage(const std::vector<std::string> &paths);
	bool pull(AbstractPtyProcess *pty = nullptr);

	bool fetch(AbstractPtyProcess *pty = nullptr, bool prune = false);
	bool reset_head1();
	bool reset_hard();
	bool clean_df();
	bool push_u(bool set_upstream, std::string const &remote, std::string const &branch, bool force, AbstractPtyProcess *pty);
	std::string objectType(const GitHash &id);
	bool rm_cached(const std::string &file);
	void cherrypick(const std::string &name);
	std::string getCherryPicking() const;
        std::vector<GitBranch> branches();

	std::string signingKey(GitSource purpose);
	bool setSigningKey(std::string const &id, bool global);
	GitSignPolicy signPolicy(GitSource source);
	bool setSignPolicy(GitSource source, GitSignPolicy policy);
	bool configGpgProgram(const std::string &path, bool global);

	bool reflog(QList<GitReflogItem> *out, int maxcount = 100);
	std::vector<char> blame(const std::string &path);

	std::optional<std::vector<GitFileItem>> ls(const std::string &path);
	std::optional<std::vector<char>> readfile(const std::string &path);
};

#endif // GITRUNNER_H
