#ifndef GITREMOTESSHSESSION_H
#define GITREMOTESSHSESSION_H
#ifdef UNSAFE_ENABLED

#include "AbstractGitSession.h"
#include "SshDialog.h"
#include <memory>

class GitRemoteSshSession : public AbstractGitSession {
public:
	std::shared_ptr<SshConnection> ssh_;
	std::string git_command_;
	bool ready_ = false;

	GitRemoteSshSession();

	std::shared_ptr<AbstractGitSession> dup();

	bool connect(std::shared_ptr<SshConnection> ssh, std::string const &gitcmd);

	bool is_connected() const;
	bool isValidWorkingCopy(QString const &dir) const;
	std::optional<GitResult> exec_git(const QString &arg, const Option &opt);
	bool remove(const QString &path);

	virtual std::optional<std::vector<GitFileItem>> ls(char const *path);
	virtual std::optional<std::vector<char>> readfile(char const *path);
};

#endif
#endif // GITREMOTESSHSESSION_H
