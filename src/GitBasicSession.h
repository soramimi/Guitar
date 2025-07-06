#ifndef GITBASICSESSION_H
#define GITBASICSESSION_H

#include "AbstractGitSession.h"
#include <QString>

class GitBasicSession : public AbstractGitSession {
private:
	QString gitCommand() const;
	QString sshCommand() const;
public:
	bool is_valid_git_command() const;


	bool exec_git(QString const &arg, Option const &opt, bool debug_ = false);
	bool pushd(std::function<bool ()> const fn);
};


#endif // GITBASICSESSION_H
