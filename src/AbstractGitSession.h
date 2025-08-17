#ifndef ABSTRACTGITSESSION_H
#define ABSTRACTGITSESSION_H

#include <vector>
#include <memory>
#include <optional>
#include <QString>
#include "GitObjectManager.h"
#include "GitTypes.h"

class GitCommandCache;
class AbstractPtyProcess;

class AbstractGitSession {
	friend class GitRunner;
public:
	struct Option {
		bool use_cache = false;
		bool chdir = true;
		bool log = true;
		bool errout = false;
		AbstractPtyProcess *pty = nullptr;
		QString prefix;
	};
	struct Info {
		QString git_command;
		QString ssh_command;// = "C:/Program Files/Git/usr/bin/ssh.exe";
		QString ssh_key_override;// = "C:/a/id_rsa";
	};
	struct Info2 {
		QString working_repo_dir;
		QString submodule_path;
		GitObjectCache object_cache;
	};

	struct GitCache;
private:
	struct Private;
	Private *m;
protected:
	void insertIntoCommandCache(QString const &key, std::vector<char> const &value);
	std::vector<char> *findFromCommandCache(QString const &key);
	virtual std::shared_ptr<AbstractGitSession> dup() = 0;
public:
	AbstractGitSession();
	virtual ~AbstractGitSession();
	AbstractGitSession(AbstractGitSession const &other);

	void clearCommandCache();

	Info &gitinfo();
	Info const &gitinfo() const;
	Info2 &gitinfo2();
	Info2 const &gitinfo2() const;

	GitCache &cache();
	QString workingDir() const;
	virtual std::optional<GitResult> exec_git(QString const &arg, Option const &opt) = 0;
	virtual bool remove(QString const &path) = 0;

	virtual bool is_connected() const = 0;
	virtual bool isValidWorkingCopy(QString const &dir) const = 0;

	virtual std::optional<std::vector<GitFileItem>> ls(char const *path) { return std::nullopt; }
	virtual std::optional<std::vector<char>> readfile(char const *path) { return std::nullopt; }
};


#endif // ABSTRACTGITSESSION_H
