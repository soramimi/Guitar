
#ifndef ABSTRACTGITSESSION_H
#define ABSTRACTGITSESSION_H

#include "GitTypes.h"
#include <memory>
#include <optional>
#include <string>
#include <vector>

class GitResult;
class GitCommandCache;
class AbstractPtyProcess;
class GitObjectCache;

class AbstractGitSession {
	friend class GitRunner;
public:
	struct Option {
		bool use_cache = false;
		bool chdir = true;
		bool log = true;
		bool errout = false;
		AbstractPtyProcess *pty = nullptr;
	};
	struct Info {
		std::string git_command;
		std::string ssh_command;// = "C:/Program Files/Git/usr/bin/ssh.exe";
		std::string ssh_key_override;// = "C:/a/id_rsa";
		std::string working_repo_dir;
		std::string submodule_path;
	};

	struct GitCache;
private:
	struct Private;
	Private *m;
protected:
	void insertIntoCommandCache(const std::string &key, std::vector<char> const &value);
	std::vector<char> *findFromCommandCache(std::string const &key);
	virtual std::shared_ptr<AbstractGitSession> dup() = 0;
public:
	AbstractGitSession();
	virtual ~AbstractGitSession();
	AbstractGitSession(AbstractGitSession const &other);

	GitObjectCache *getObjectCache();
	void clearCommandCache();
	void clearObjectCache();

	Info &gitinfo();
	Info const &gitinfo() const;
	// Info2 &gitinfo2();
	// Info2 const &gitinfo2() const;

	// GitCache &cache();
	std::string workingDir() const;
	virtual std::optional<GitResult> exec_git(std::string const &arg, Option const &opt) = 0;
	virtual bool remove(std::string const &path) = 0;

	virtual bool is_connected() const = 0;
	virtual bool isValidWorkingCopy(std::string const &dir) const = 0;

	virtual std::optional<std::vector<GitFileItem>> ls(char const *path) { return std::nullopt; }
	virtual std::optional<std::vector<char>> readfile(char const *path) { return std::nullopt; }
};

#endif // ABSTRACTGITSESSION_H
