#ifndef GITBASICSESSION_H
#define GITBASICSESSION_H

#include "AbstractGitSession.h"
#include <QString>

class GitBasicSession : public AbstractGitSession {
public:
	struct Commands {
		QString git_command;
		QString ssh_command;
	};
private:
	QString gitCommand() const;
	QString sshCommand() const;
public:
	GitBasicSession(Commands const &cmds);

	bool is_connected() const;
	bool isValidWorkingCopy(QString const &dir) const;
	std::optional<AbstractGitSession::GitResult> exec_git(QString const &arg, Option const &opt);
	bool remove(const QString &path);

	virtual std::optional<std::vector<GitFileItem>> ls(char const *path);
	virtual std::optional<std::vector<char>> readfile(char const *path);
};

#endif // GITBASICSESSION_H
