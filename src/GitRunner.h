#ifndef GITRUNNER_H
#define GITRUNNER_H

#include <memory>
#include <optional>
#include <QString>
#include "GitTypes.h"

class Git;

class GitRunner {
public:
	std::shared_ptr<Git> git;
	GitRunner() = default;
	GitRunner(std::shared_ptr<Git> const &git);
	GitRunner(GitRunner const &that);
	GitRunner(GitRunner &&that);
	void operator = (GitRunner const &that);
	operator bool () const;
	GitRunner dup() const;

	void clearCommandCache();

	static std::optional<GitCommitItem> parseCommit(QByteArray const &ba);

	bool isValidWorkingCopy(QString const &dir) const;

	bool isValidWorkingCopy() const;

	void setWorkingRepositoryDir(QString const &repo, const QString &sshkey);
	void setSubmodulePath(const QString &submodpath);
	QString workingDir() const;
	QString const &sshKey() const;
	void setSshKey(const QString &sshkey) const;

	QString getMessage(const QString &id);
	QString errorMessage(std::optional<GitResult> const &var) const;

	bool remove(QString const &path);

	GitHash rev_parse(QString const &name);
	void setRemoteURL(const GitRemote &remote);
	void addRemoteURL(const GitRemote &remote);
	void removeRemote(QString const &name);
	QStringList getRemotes();

	QString version();

	bool init();

	QList<GitTag> tags();
	bool tag(QString const &name, GitHash const &id = {});
	bool delete_tag(QString const &name, bool remote);

	void resetFile(QString const &path);
	void resetAllFiles();

	void removeFile(QString const &path, bool rm_real_file);

	GitUser getUser(GitSource purpose);
	void setUser(GitUser const&user, bool global);
	QString getDefaultBranch();
	void setDefaultBranch(QString const &branchname);
	void unsetDefaultBranch();
	QDateTime repositoryLastModifiedTime();
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

	GitCommitItemList log_all(GitHash const &id, int maxcount);
	GitCommitItemList log_file(QString const &path, int maxcount);
	QStringList rev_list_all(GitHash const &id, int maxcount);

	std::optional<GitCommitItem> log_signature(GitHash const &id);
	GitCommitItemList log(int maxcount);
	std::optional<GitCommitItem> queryCommitItem(const GitHash &id);

	bool stash();
	bool stash_apply();
	bool stash_drop();

	QList<GitSubmoduleItem> submodules();
	bool submodule_add(const GitCloneData &data, bool force, AbstractPtyProcess *pty);
	bool submodule_update(const GitSubmoduleUpdateData &data, AbstractPtyProcess *pty);
	QString queryEntireCommitMessage(const GitHash &id);

	QList<GitDiffRaw> diff_raw(GitHash const &old_id, GitHash const &new_id);
	std::string diff_head(std::function<bool (std::string const &name, std::string const &mime)> fn_accept = nullptr);
	QString diff(QString const &old_id, QString const &new_id);
	QString diff_file(QString const &old_path, QString const &new_path);
	QString diff_to_file(QString const &old_id, QString const &path);

	std::vector<GitFileStatus> status_s();
	std::optional<QByteArray> cat_file(const GitHash &id);
	bool clone(GitCloneData const &data, AbstractPtyProcess *pty);
	void add_A();
	bool unstage_all();

	void stage(QString const &path);
	bool stage(QStringList const &paths, AbstractPtyProcess *pty);
	void unstage(QString const &path);
	void unstage(QStringList const &paths);
	bool pull(AbstractPtyProcess *pty = nullptr);

	bool fetch(AbstractPtyProcess *pty = nullptr, bool prune = false);
	bool reset_head1();
	bool reset_hard();
	bool clean_df();
	bool push_u(bool set_upstream, QString const &remote, QString const &branch, bool force, AbstractPtyProcess *pty);
	QString objectType(const GitHash &id);
	bool rm_cached(QString const &file);
	void cherrypick(QString const &name);
	QString getCherryPicking() const;
	QList<GitBranch> branches();

	QString signingKey(GitSource purpose);
	bool setSigningKey(QString const &id, bool global);
	GitSignPolicy signPolicy(GitSource source);
	bool setSignPolicy(GitSource source, GitSignPolicy policy);
	bool configGpgProgram(QString const &path, bool global);

	bool reflog(QList<GitReflogItem> *out, int maxcount = 100);
	QByteArray blame(QString const &path);

	std::optional<std::vector<GitFileItem>> ls(const QString &path);
	std::optional<std::vector<char>> readfile(const QString &path);
};

#endif // GITRUNNER_H
