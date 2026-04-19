#include "GitRunner.h"
#include "Git.h"
#include "TraceLogger.h"
#include "common/q/helper.h"

GitRunner::GitRunner(const std::shared_ptr<Git> &git)
	: git(git)
{
}

GitRunner::GitRunner(const GitRunner &that)
	: git(that.git)
{
}

GitRunner::GitRunner(GitRunner &&that)
	: git(std::move(that.git))
{
}

void GitRunner::operator =(const GitRunner &that)
{
	git = that.git;
}

GitRunner::operator bool() const
{
	return (bool)git;
}

GitRunner GitRunner::dup() const
{
	Git *newgit = new Git();
	newgit->session_ = gitptr()->session_->dup();
	return std::shared_ptr<Git>(newgit);
}

GitObjectCache *GitRunner::getObjCache()
{
	Q_ASSERT(git);
	return gitptr()->session_->getObjectCache();
}

void GitRunner::clearCommandCache()
{
	if (git) gitptr()->clearCommandCache();
}

void GitRunner::clearObjectCache()
{
	if (git) gitptr()->clearObjectCache();
}

std::optional<GitCommitItem> GitRunner::parseCommit(std::vector<char> const &ba)
{
	return Git::parseCommit(ba);
}

bool GitRunner::isValidWorkingCopy(std::string const &dir) const
{
	return git && gitptr()->isValidWorkingCopy(dir);
}

bool GitRunner::isValidWorkingCopy() const
{
	return git && gitptr()->isValidWorkingCopy();
}

void GitRunner::setWorkingRepositoryDir(std::string const &repo, std::string const &sshkey)
{
	gitptr()->setWorkingRepositoryDir(repo, sshkey);
}

void GitRunner::setSubmodulePath(std::string const &submodpath)
{
	gitptr()->setSubmodulePath(submodpath);
}

std::string GitRunner::workingDir() const
{
	return gitptr()->workingDir();
}

const std::string &GitRunner::sshKey() const
{
	return gitptr()->sshKey();
}

void GitRunner::setSshKey(std::string const &sshkey)
{
	gitptr()->setSshKey(sshkey);
}

std::string GitRunner::getMessage(std::string const &id)
{
	return gitptr()->getMessage(id);
}

std::string GitRunner::errorMessage(const std::optional<GitResult> &var) const
{
	return gitptr()->errorMessage(var);
}

bool GitRunner::remove(std::string const &path)
{
	return gitptr()->remove(path);
}

GitHash GitRunner::rev_parse(std::string const &name, bool use_cache)
{
	if (name == "HEAD") {
		use_cache = false;
	}
	if (use_cache) {
		GitObjectCache *cache = getObjCache();
		Q_ASSERT(cache);
		return cache->rev_parse(*this, name);
	} else {
		return gitptr()->rev_parse(name);
	}
}

void GitRunner::setRemoteURL(const GitRemote &remote)
{
	gitptr()->setRemoteURL(remote);
}

void GitRunner::addRemoteURL(const GitRemote &remote)
{
	gitptr()->addRemoteURL(remote);
}

void GitRunner::removeRemote(std::string const &name)
{
	gitptr()->removeRemote(name);
}

std::vector<std::string> GitRunner::getRemotes()
{
	return gitptr()->getRemotes();
}

std::string GitRunner::version()
{
	return gitptr()->version();
}

bool GitRunner::init()
{
	return gitptr()->init();
}

std::vector<GitTag> GitRunner::tags()
{
	return gitptr()->tags();
}

bool GitRunner::tag(std::string const &name, const GitHash &id)
{
	return gitptr()->tag(name, id);
}

bool GitRunner::delete_tag(std::string const &name, bool remote)
{
	return gitptr()->delete_tag(name, remote);
}

void GitRunner::resetFile(std::string const &path)
{
	gitptr()->resetFile(path);
}

void GitRunner::resetAllFiles()
{
	gitptr()->resetAllFiles();
}

void GitRunner::removeFile(std::string const &path, bool rm_real_file)
{
	gitptr()->rm(path, rm_real_file);
}

GitUser GitRunner::getUser(GitSource purpose)
{
	if (!git) return {}; // gitコマンドが存在しない場合は空のオブジェクトを返し、ASSERTでは落とさない。
	return gitptr()->getUser(purpose);
}

void GitRunner::setUser(const GitUser &user, bool global)
{
	gitptr()->setUser(user, global);
}

std::string GitRunner::getDefaultBranch()
{
	return gitptr()->getDefaultBranch();
}

void GitRunner::setDefaultBranch(std::string const &branchname)
{
	gitptr()->setDefaultBranch(branchname);
}

void GitRunner::unsetDefaultBranch()
{
	gitptr()->unsetDefaultBranch();
}

QDateTime GitRunner::repositoryLastModifiedTime()
{
	return gitptr()->repositoryLastModifiedTime();
}

std::string GitRunner::status()
{
	return gitptr()->status();
}

bool GitRunner::commit(std::string const &text, bool sign, AbstractPtyProcess *pty)
{
	return gitptr()->commit(text, sign, pty);
}

bool GitRunner::commit_amend_m(std::string const &text, bool sign, AbstractPtyProcess *pty)
{
	return gitptr()->commit_amend_m(text, sign, pty);
}

bool GitRunner::revert(const GitHash &id)
{
	return gitptr()->revert(id);
}

bool GitRunner::push_tags(AbstractPtyProcess *pty)
{
	return gitptr()->push_tags(pty);
}

void GitRunner::remote_v(std::vector<GitRemote> *out)
{
	gitptr()->remote_v(out);
}

void GitRunner::createBranch(std::string const &name)
{
	gitptr()->createBranch(name);
}

void GitRunner::checkoutBranch(std::string const &name)
{
	gitptr()->checkoutBranch(name);
}

void GitRunner::mergeBranch(std::string const &name, GitMergeFastForward ff, bool squash)
{
	gitptr()->mergeBranch(name, ff, squash);
}

bool GitRunner::deleteBranch(std::string const &name)
{
	return gitptr()->deleteBranch(name);
}

bool GitRunner::checkout(const std::string &branch_name, const std::string &id)
{
	return gitptr()->checkout(branch_name, id);
}

bool GitRunner::checkout_detach(const std::string &id)
{
	return gitptr()->checkout_detach(id);
}

void GitRunner::rebaseBranch(std::string const &name)
{
	gitptr()->rebaseBranch(name);
}

void GitRunner::rebase_abort()
{
	gitptr()->rebase_abort();
}

GitCommitItemList GitRunner::log_all(const GitHash &id, int maxcount)
{
	return gitptr()->log_all(id, maxcount);
}

GitCommitItemList GitRunner::log_file(std::string const &path, int maxcount)
{
	return gitptr()->log_file(path, maxcount);
}

std::vector<GitHash> GitRunner::rev_list_all(const GitHash &id, int maxcount)
{
	return gitptr()->rev_list_all(id, maxcount);
}

std::optional<GitCommitItem> GitRunner::log_signature(const GitHash &id)
{
	return gitptr()->log_signature(id);
}

GitCommitItemList GitRunner::log(int maxcount)
{
	return gitptr()->log(maxcount);
}

std::optional<GitCommitItem> GitRunner::queryCommitItem(const GitHash &id)
{
	return gitptr()->queryCommitItem(id);
}

bool GitRunner::stash()
{
	return gitptr()->stash();
}

bool GitRunner::stash_apply()
{
	return gitptr()->stash_apply();
}

bool GitRunner::stash_drop()
{
	return gitptr()->stash_drop();
}

QList<GitSubmoduleItem> GitRunner::submodules()
{
	return gitptr()->submodules();
}

bool GitRunner::submodule_add(const GitCloneData &data, bool force, AbstractPtyProcess *pty)
{
	return gitptr()->submodule_add(data, force, pty);
}

bool GitRunner::submodule_update(const GitSubmoduleUpdateData &data, AbstractPtyProcess *pty)
{
	return gitptr()->submodule_update(data, pty);
}

std::string GitRunner::queryEntireCommitMessage(const GitHash &id)
{
	return gitptr()->queryEntireCommitMessage(id);
}

std::vector<GitDiffRaw> GitRunner::diff_raw(const GitHash &old_id, const GitHash &new_id)
{
	return gitptr()->diff_raw(old_id, new_id);
}

std::string GitRunner::diff(std::string const &old_id, std::string const &new_id)
{
	return gitptr()->diff(old_id, new_id);
}

std::string GitRunner::diff_file(std::string const &old_path, std::string const &new_path)
{
	return gitptr()->diff_file(old_path, new_path);
}

std::string GitRunner::diff_to_file(std::string const &old_id, std::string const &path)
{
	return gitptr()->diff_to_file(old_id, path);
}

std::vector<std::string> GitRunner::diff_name_only_head()
{
	return gitptr()->diff_name_only_head();
}

std::string GitRunner::diff_full_index_head_file(std::string const &file)
{
	return gitptr()->diff_full_index_head_file(file);
}

std::vector<GitFileStatus> GitRunner::status_s()
{
	return gitptr()->status_s();
}

std::optional<std::vector<char>> GitRunner::cat_file_(const GitHash &id)
{
	return gitptr()->cat_file(id);
}

GitObject GitRunner::catFile(const GitHash &id, bool use_cache)
{
	Q_ASSERT(git);
	if (use_cache) {
		GitObjectCache *cache = getObjCache();
		Q_ASSERT(cache);
		return cache->catFile(*this, id);
	} else {
		GitObject obj;
		auto data = gitptr()->cat_file(id);
		if (data) {
			obj.content = (QBA)*data;
			obj.type = GitObject::Type::UNKNOWN;
		}
		return obj;
	}
}

bool GitRunner::clone(const GitCloneData &data, AbstractPtyProcess *pty)
{
	return gitptr()->clone(data, pty);
}

void GitRunner::add_A()
{
	gitptr()->add_A();
}

bool GitRunner::unstage_all()
{
	return gitptr()->unstage_all();
}

void GitRunner::stage(std::string const &path)
{
	gitptr()->stage(path);
}

bool GitRunner::stage(std::vector<std::string> const &paths, AbstractPtyProcess *pty)
{
	return gitptr()->stage(paths, pty);
}

void GitRunner::unstage(std::string const &path)
{
	gitptr()->unstage(path);
}

void GitRunner::unstage(std::vector<std::string> const &paths)
{
	gitptr()->unstage(paths);
}

bool GitRunner::pull(AbstractPtyProcess *pty)
{
	return gitptr()->pull(pty);
}

bool GitRunner::fetch(AbstractPtyProcess *pty, bool prune)
{
	return gitptr()->fetch(pty, prune);
}

bool GitRunner::reset_head1()
{
	return gitptr()->reset_head1();
}

bool GitRunner::reset_hard()
{
	return gitptr()->reset_hard();
}

bool GitRunner::clean_df()
{
	return gitptr()->clean_df();
}

bool GitRunner::push_u(bool set_upstream, std::string const &remote, std::string const &branch, bool force, AbstractPtyProcess *pty)
{
	return gitptr()->push_u(set_upstream, remote, branch, force, pty);
}

std::string GitRunner::objectType(const GitHash &id)
{
	return gitptr()->objectType(id);
}

bool GitRunner::rm_cached(std::string const &file)
{
	return gitptr()->rm_cached(file);
}

void GitRunner::cherrypick(std::string const &name)
{
	gitptr()->cherrypick(name);
}

std::string GitRunner::getCherryPicking() const
{
	return gitptr()->getCherryPicking();
}

std::vector<GitBranch> GitRunner::branches()
{
	return gitptr()->branches();
}

std::string GitRunner::signingKey(GitSource purpose)
{
	return gitptr()->signingKey(purpose);
}

bool GitRunner::setSigningKey(std::string const &id, bool global)
{
	return gitptr()->setSigningKey(id, global);
}

GitSignPolicy GitRunner::signPolicy(GitSource source)
{
	return gitptr()->signPolicy(source);
}

bool GitRunner::setSignPolicy(GitSource source, GitSignPolicy policy)
{
	return gitptr()->setSignPolicy(source, policy);
}

bool GitRunner::configGpgProgram(std::string const &path, bool global)
{
	return gitptr()->configGpgProgram(path, global);
}

bool GitRunner::reflog(QList<GitReflogItem> *out, int maxcount)
{
	return gitptr()->reflog(out, maxcount);
}

std::vector<char> GitRunner::blame(std::string const &path)
{
	return gitptr()->blame(path);
}

std::optional<std::vector<GitFileItem>> GitRunner::ls(std::string const &path)
{
	return gitptr()->ls(path);
}

std::optional<std::vector<char>> GitRunner::readfile(std::string const &path)
{
	return gitptr()->readfile(path);
}
