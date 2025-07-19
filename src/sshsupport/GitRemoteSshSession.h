#ifndef GITREMOTESSHSESSION_H
#define GITREMOTESSHSESSION_H

#include "AbstractGitSession.h"
#include "SshDialog.h"
#include <memory>

class GitRemoteSshSession : public AbstractGitSession {
public:
	std::shared_ptr<SshConnection> ssh_;
	std::string git_command_;
	bool ready_ = false;

	GitRemoteSshSession();

	bool connect(std::shared_ptr<SshConnection> ssh, std::string const &gitcmd);

	bool is_connected() const;
	bool isValidWorkingCopy(QString const &dir) const;
	bool exec_git(const QString &arg, const Option &opt);
	bool remove(const QString &path);

	bool ls(const char *path, std::vector<GitFileItem> *files);
	bool readfile(char const *path, std::vector<char> *data);
};

#endif // GITREMOTESSHSESSION_H
