#include "GitRunner.h"
#include "Git.h"

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
	newgit->session_ = git->session_->dup();
	return std::shared_ptr<Git>(newgit);
}

void GitRunner::clearCommandCache()
{
	if (git) git->clearCommandCache();
}

std::optional<GitCommitItem> GitRunner::parseCommit(const QByteArray &ba)
{
	return Git::parseCommit(ba);
}

bool GitRunner::isValidWorkingCopy(const QString &dir) const
{
	return git && git->isValidWorkingCopy(dir);
}

bool GitRunner::isValidWorkingCopy() const
{
	return git && git->isValidWorkingCopy();
}

void GitRunner::setWorkingRepositoryDir(const QString &repo, const QString &sshkey)
{
	Q_ASSERT(git);
	git->setWorkingRepositoryDir(repo, sshkey);
}

void GitRunner::setSubmodulePath(const QString &submodpath)
{
	Q_ASSERT(git);
	git->setSubmodulePath(submodpath);
}

QString GitRunner::workingDir() const
{
	Q_ASSERT(git);
	return git->workingDir();
}

const QString &GitRunner::sshKey() const
{
	Q_ASSERT(git);
	return git->sshKey();
}

void GitRunner::setSshKey(const QString &sshkey) const
{
	Q_ASSERT(git);
	git->setSshKey(sshkey);
}

QString GitRunner::getMessage(const QString &id)
{
	Q_ASSERT(git);
	return git->getMessage(id);
}

QString GitRunner::errorMessage(const std::optional<GitResult> &var) const
{
	Q_ASSERT(git);
	return git->errorMessage(var);
}

bool GitRunner::remove(const QString &path)
{
	Q_ASSERT(git);
	return git->remove(path);
}

GitHash GitRunner::rev_parse(const QString &name)
{
	Q_ASSERT(git);
	return git->rev_parse(name);
}

void GitRunner::setRemoteURL(const GitRemote &remote)
{
	Q_ASSERT(git);
	git->setRemoteURL(remote);
}

void GitRunner::addRemoteURL(const GitRemote &remote)
{
	Q_ASSERT(git);
	git->addRemoteURL(remote);
}

void GitRunner::removeRemote(const QString &name)
{
	Q_ASSERT(git);
	git->removeRemote(name);
}

QStringList GitRunner::getRemotes()
{
	Q_ASSERT(git);
	return git->getRemotes();
}

QString GitRunner::version()
{
	Q_ASSERT(git);
	return git->version();
}

bool GitRunner::init()
{
	Q_ASSERT(git);
	return git->init();
}

QList<GitTag> GitRunner::tags()
{
	Q_ASSERT(git);
	return git->tags();
}

bool GitRunner::tag(const QString &name, const GitHash &id)
{
	Q_ASSERT(git);
	return git->tag(name, id);
}

bool GitRunner::delete_tag(const QString &name, bool remote)
{
	Q_ASSERT(git);
	return git->delete_tag(name, remote);
}

void GitRunner::resetFile(const QString &path)
{
	Q_ASSERT(git);
	git->resetFile(path);
}

void GitRunner::resetAllFiles()
{
	Q_ASSERT(git);
	git->resetAllFiles();
}

void GitRunner::removeFile(const QString &path, bool rm_real_file)
{
	Q_ASSERT(git);
	git->rm(path, rm_real_file);
}

GitUser GitRunner::getUser(GitSource purpose)
{
	Q_ASSERT(git);
	return git->getUser(purpose);
}

void GitRunner::setUser(const GitUser &user, bool global)
{
	Q_ASSERT(git);
	git->setUser(user, global);
}

QString GitRunner::getDefaultBranch()
{
	Q_ASSERT(git);
	return git->getDefaultBranch();
}

void GitRunner::setDefaultBranch(const QString &branchname)
{
	Q_ASSERT(git);
	git->setDefaultBranch(branchname);
}

void GitRunner::unsetDefaultBranch()
{
	Q_ASSERT(git);
	git->unsetDefaultBranch();
}

QDateTime GitRunner::repositoryLastModifiedTime()
{
	Q_ASSERT(git);
	return git->repositoryLastModifiedTime();
}

QString GitRunner::status()
{
	Q_ASSERT(git);
	return git->status();
}

bool GitRunner::commit(const QString &text, bool sign, AbstractPtyProcess *pty)
{
	Q_ASSERT(git);
	return git->commit(text, sign, pty);
}

bool GitRunner::commit_amend_m(const QString &text, bool sign, AbstractPtyProcess *pty)
{
	Q_ASSERT(git);
	return git->commit_amend_m(text, sign, pty);
}

bool GitRunner::revert(const GitHash &id)
{
	Q_ASSERT(git);
	return git->revert(id);
}

bool GitRunner::push_tags(AbstractPtyProcess *pty)
{
	Q_ASSERT(git);
	return git->push_tags(pty);
}

void GitRunner::remote_v(std::vector<GitRemote> *out)
{
	Q_ASSERT(git);
	git->remote_v(out);
}

void GitRunner::createBranch(const QString &name)
{
	Q_ASSERT(git);
	git->createBranch(name);
}

void GitRunner::checkoutBranch(const QString &name)
{
	Q_ASSERT(git);
	git->checkoutBranch(name);
}

void GitRunner::mergeBranch(const QString &name, GitMergeFastForward ff, bool squash)
{
	Q_ASSERT(git);
	git->mergeBranch(name, ff, squash);
}

bool GitRunner::deleteBranch(const QString &name)
{
	Q_ASSERT(git);
	return git->deleteBranch(name);
}

bool GitRunner::checkout(const QString &branch_name, const QString &id)
{
	Q_ASSERT(git);
	return git->checkout(branch_name, id);
}

bool GitRunner::checkout_detach(const QString &id)
{
	Q_ASSERT(git);
	return git->checkout_detach(id);
}

void GitRunner::rebaseBranch(const QString &name)
{
	Q_ASSERT(git);
	git->rebaseBranch(name);
}

void GitRunner::rebase_abort()
{
	Q_ASSERT(git);
	git->rebase_abort();
}

GitCommitItemList GitRunner::log_all(const GitHash &id, int maxcount)
{
	Q_ASSERT(git);
	return git->log_all(id, maxcount);
}

GitCommitItemList GitRunner::log_file(const QString &path, int maxcount)
{
	Q_ASSERT(git);
	return git->log_file(path, maxcount);
}

QStringList GitRunner::rev_list_all(const GitHash &id, int maxcount)
{
	Q_ASSERT(git);
	return git->rev_list_all(id, maxcount);
}

std::optional<GitCommitItem> GitRunner::log_signature(const GitHash &id)
{
	Q_ASSERT(git);
	return git->log_signature(id);
}

GitCommitItemList GitRunner::log(int maxcount)
{
	Q_ASSERT(git);
	return git->log(maxcount);
}

std::optional<GitCommitItem> GitRunner::queryCommitItem(const GitHash &id)
{
	Q_ASSERT(git);
	return git->queryCommitItem(id);
}

bool GitRunner::stash()
{
	Q_ASSERT(git);
	return git->stash();
}

bool GitRunner::stash_apply()
{
	Q_ASSERT(git);
	return git->stash_apply();
}

bool GitRunner::stash_drop()
{
	Q_ASSERT(git);
	return git->stash_drop();
}

QList<GitSubmoduleItem> GitRunner::submodules()
{
	Q_ASSERT(git);
	return git->submodules();
}

bool GitRunner::submodule_add(const GitCloneData &data, bool force, AbstractPtyProcess *pty)
{
	Q_ASSERT(git);
	return git->submodule_add(data, force, pty);
}

bool GitRunner::submodule_update(const GitSubmoduleUpdateData &data, AbstractPtyProcess *pty)
{
	Q_ASSERT(git);
	return git->submodule_update(data, pty);
}

QString GitRunner::queryEntireCommitMessage(const GitHash &id)
{
	Q_ASSERT(git);
	return git->queryEntireCommitMessage(id);
}

QList<GitDiffRaw> GitRunner::diff_raw(const GitHash &old_id, const GitHash &new_id)
{
	Q_ASSERT(git);
	return git->diff_raw(old_id, new_id);
}

std::string GitRunner::diff_head(std::function<bool (const std::string &, const std::string &)> fn_accept)
{
	Q_ASSERT(git);
	return git->diff_head(fn_accept);
}

QString GitRunner::diff(const QString &old_id, const QString &new_id)
{
	Q_ASSERT(git);
	return git->diff(old_id, new_id);
}

QString GitRunner::diff_file(const QString &old_path, const QString &new_path)
{
	Q_ASSERT(git);
	return git->diff_file(old_path, new_path);
}

QString GitRunner::diff_to_file(const QString &old_id, const QString &path)
{
	Q_ASSERT(git);
	return git->diff_to_file(old_id, path);
}

std::vector<GitFileStatus> GitRunner::status_s()
{
	Q_ASSERT(git);
	return git->status_s();
}

std::optional<QByteArray> GitRunner::cat_file(const GitHash &id)
{
	Q_ASSERT(git);
	return git->cat_file(id);
}

bool GitRunner::clone(const GitCloneData &data, AbstractPtyProcess *pty)
{
	Q_ASSERT(git);
	return git->clone(data, pty);
}

void GitRunner::add_A()
{
	Q_ASSERT(git);
	git->add_A();
}

bool GitRunner::unstage_all()
{
	Q_ASSERT(git);
	return git->unstage_all();
}

void GitRunner::stage(const QString &path)
{
	Q_ASSERT(git);
	git->stage(path);
}

bool GitRunner::stage(const QStringList &paths, AbstractPtyProcess *pty)
{
	Q_ASSERT(git);
	return git->stage(paths, pty);
}

void GitRunner::unstage(const QString &path)
{
	Q_ASSERT(git);
	git->unstage(path);
}

void GitRunner::unstage(const QStringList &paths)
{
	Q_ASSERT(git);
	git->unstage(paths);
}

bool GitRunner::pull(AbstractPtyProcess *pty)
{
	Q_ASSERT(git);
	return git->pull(pty);
}

bool GitRunner::fetch(AbstractPtyProcess *pty, bool prune)
{
	Q_ASSERT(git);
	return git->fetch(pty, prune);
}

bool GitRunner::reset_head1()
{
	Q_ASSERT(git);
	return git->reset_head1();
}

bool GitRunner::reset_hard()
{
	Q_ASSERT(git);
	return git->reset_hard();
}

bool GitRunner::clean_df()
{
	Q_ASSERT(git);
	return git->clean_df();
}

bool GitRunner::push_u(bool set_upstream, const QString &remote, const QString &branch, bool force, AbstractPtyProcess *pty)
{
	Q_ASSERT(git);
	return git->push_u(set_upstream, remote, branch, force, pty);
}

QString GitRunner::objectType(const GitHash &id)
{
	Q_ASSERT(git);
	return git->objectType(id);
}

bool GitRunner::rm_cached(const QString &file)
{
	Q_ASSERT(git);
	return git->rm_cached(file);
}

void GitRunner::cherrypick(const QString &name)
{
	Q_ASSERT(git);
	git->cherrypick(name);
}

QString GitRunner::getCherryPicking() const
{
	Q_ASSERT(git);
	return git->getCherryPicking();
}

QList<GitBranch> GitRunner::branches()
{
	Q_ASSERT(git);
	return git->branches();
}

QString GitRunner::signingKey(GitSource purpose)
{
	Q_ASSERT(git);
	return git->signingKey(purpose);
}

bool GitRunner::setSigningKey(const QString &id, bool global)
{
	Q_ASSERT(git);
	return git->setSigningKey(id, global);
}

GitSignPolicy GitRunner::signPolicy(GitSource source)
{
	Q_ASSERT(git);
	return git->signPolicy(source);
}

bool GitRunner::setSignPolicy(GitSource source, GitSignPolicy policy)
{
	Q_ASSERT(git);
	return git->setSignPolicy(source, policy);
}

bool GitRunner::configGpgProgram(const QString &path, bool global)
{
	Q_ASSERT(git);
	return git->configGpgProgram(path, global);
}

bool GitRunner::reflog(QList<GitReflogItem> *out, int maxcount)
{
	Q_ASSERT(git);
	return git->reflog(out, maxcount);
}

QByteArray GitRunner::blame(const QString &path)
{
	Q_ASSERT(git);
	return git->blame(path);
}

std::optional<std::vector<GitFileItem> > GitRunner::ls(const QString &path)
{
	Q_ASSERT(git);
	return git->ls(path);
}

std::optional<std::vector<char> > GitRunner::readfile(const QString &path)
{
	Q_ASSERT(git);
	return git->readfile(path);
}
