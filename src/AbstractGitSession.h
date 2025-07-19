#ifndef ABSTRACTGITSESSION_H
#define ABSTRACTGITSESSION_H

#include <map>
#include <vector>
#include <memory>
#include <QString>
#include "MyProcess.h"
#include "GitTypes.h"

class GitCommandCache;

class AbstractGitSession {
public:
	struct Option {
		bool use_cache = false;
		bool chdir = true;
		bool log = false;
		bool errout = false;
		AbstractPtyProcess *pty = nullptr;
		QString prefix;
	};
	struct Info {
		QString git_command;
		QString working_repo_dir;
		QString submodule_path;
		QString ssh_command;// = "C:/Program Files/Git/usr/bin/ssh.exe";
		QString ssh_key_override;// = "C:/a/id_rsa";
	};
	struct Var {
		std::vector<char> result;
		ProcessStatus exit_status;
	};

	struct GitCache;
private:
	struct Private;
	Private *m;
protected:
	void insertIntoCommandCache(QString const &key, std::vector<char> const &value);
	std::vector<char> *findFromCommandCache(QString const &key);
public:
	AbstractGitSession();
	virtual ~AbstractGitSession();

	Var &var();
	Var const &var() const;
	Info &gitinfo();
	Info const &gitinfo() const;

	GitCache &cache();
	void clearResult();
	QString workingDir() const;
	virtual bool exec_git(QString const &arg, Option const &opt) = 0;
	// virtual bool pushd(std::function<bool ()> const fn) = 0;
	virtual bool remove(QString const &path) = 0;

	virtual bool is_connected() const = 0;
	virtual bool isValidWorkingCopy(QString const &dir) const = 0;

	virtual bool ls(char const *path, std::vector<GitFileItem> *files) { return false; }
	virtual bool readfile(char const *path, std::vector<char> *data) { return false; }

	void set_command_cache(GitCommandCache const &cc);
};


#endif // ABSTRACTGITSESSION_H
