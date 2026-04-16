#ifndef GITBASICSESSION_H
#define GITBASICSESSION_H

#include "AbstractGitSession.h"
#include <QString>

class GitBasicSession : public AbstractGitSession {
public:
	struct Commands {
		std::string git_command;
		std::string ssh_command;
	};
private:
	std::string gitCommand() const;
	std::string sshCommand() const;
protected:
	virtual std::shared_ptr<AbstractGitSession> dup();
public:
	GitBasicSession(Commands const &cmds);

	bool is_connected() const;
	bool isValidWorkingCopy(std::string const &dir) const;
	std::optional<GitResult> exec_git(std::string const &arg, Option const &opt);
	bool remove(const std::string &path);

	virtual std::optional<std::vector<GitFileItem>> ls(char const *path);
	virtual std::optional<std::vector<char>> readfile(char const *path);
};

#endif // GITBASICSESSION_H
