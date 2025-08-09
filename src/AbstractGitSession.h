#ifndef ABSTRACTGITSESSION_H
#define ABSTRACTGITSESSION_H

#include <map>
#include <vector>
#include <memory>
#include <optional>
#include <QString>
#include "MyProcess.h"
#include "GitTypes.h"

class GitCommandCache;
class AbstractPtyProcess;

class GitResult {
private:
	ProcessStatus status_;
public:
	void set_exit_code(int code)
	{
		status_.exit_code = code;
	}
	void set_output(std::vector<char> const &out)
	{
		status_.output = out;
	}
	void set_error_message(std::string const &msg)
	{
		status_.error_message = msg;
	}

	bool ok() const
	{
		return status_.ok;
	}
	int exit_code()
	{
		return status_.exit_code;
	}
	std::vector<char> const &output() const
	{
		return status_.output;
	}
	std::string error_message() const
	{
		return status_.error_message;
	}
	std::string log_message() const
	{
		return status_.log_message;
	}
};

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

	void clearCommandCache();

	Info &gitinfo();
	Info const &gitinfo() const;

	GitCache &cache();
	QString workingDir() const;
	virtual std::optional<GitResult> exec_git(QString const &arg, Option const &opt) = 0;
	virtual bool remove(QString const &path) = 0;

	virtual bool is_connected() const = 0;
	virtual bool isValidWorkingCopy(QString const &dir) const = 0;

	virtual std::optional<std::vector<GitFileItem>> ls(char const *path) { return std::nullopt; }
	virtual std::optional<std::vector<char>> readfile(char const *path) { return std::nullopt; }

	void set_command_cache(GitCommandCache const &cc);
};


#endif // ABSTRACTGITSESSION_H
