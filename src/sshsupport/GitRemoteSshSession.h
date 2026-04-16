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
	bool isValidWorkingCopy(std::string const &dir) const;
	std::optional<GitResult> exec_git(std::string const &arg, const Option &opt);
	bool remove(std::string const &path);

	virtual std::optional<std::vector<GitFileItem>> ls(char const *path);
	virtual std::optional<std::vector<char>> readfile(char const *path);
};

#endif
#endif // GITREMOTESSHSESSION_H
