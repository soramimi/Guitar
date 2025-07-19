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
	bool exec_git(QString const &arg, Option const &opt);
	// bool pushd(std::function<bool ()> const fn);
	bool remove(const QString &path);

	virtual bool ls(char const *path, std::vector<GitFileItem> *files);
	virtual bool readfile(char const *path, std::vector<char> *data);
};

#endif // GITBASICSESSION_H
