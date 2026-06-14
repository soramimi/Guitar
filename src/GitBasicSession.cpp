#include "GitBasicSession.h"
#include "TraceEventWriter.h"
#include <common/fmt.h>
#include <common/str.h>
#include <common/npos.h>
#include <common/joinpath.h>
#include <common/q/FileInfo.h>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFileInfo>

#ifdef APP_GUITAR
#include "ApplicationGlobal.h"
static inline void global_write_log(std::string_view const &s)
{
	global->writeLog(s);
}
#else
void global_write_log(std::string_view const &s);
#endif

GitBasicSession::GitBasicSession(const Commands &cmds)
{
	gitinfo().git_command = cmds.git_command;
	gitinfo().ssh_command = cmds.ssh_command;
}

std::string GitBasicSession::gitCommand() const
{
	return gitinfo().git_command;
}

std::string GitBasicSession::sshCommand() const
{
	return gitinfo().ssh_command;
}

std::shared_ptr<AbstractGitSession> GitBasicSession::dup()
{
	std::shared_ptr<AbstractGitSession> ptr(new GitBasicSession(*this));
	return ptr;
}

bool GitBasicSession::is_connected() const
{
	return QFileInfo(QString::fromStdString(gitCommand())).isExecutable();
}

std::optional<GitResult> GitBasicSession::exec_git(std::string const &arg, const Option &opt)
{
	std::string cmd = gitCommand();
	FileInfo info2(cmd);
	if (!info2.isExecutable()) {
		qDebug() << "Invalid git command: " << QString::fromStdString(gitCommand());
		return std::nullopt;
	}

	QString env;
	std::string ssh = sshCommand();
	if (ssh.empty() || gitinfo().ssh_key_override.empty()) {
		// nop
	} else {
		if ((FOUND)ssh.find('\"')) return std::nullopt;
		if ((FOUND)gitinfo().ssh_key_override.find('\"')) return std::nullopt;
		if (!QFileInfo(QString::fromStdString(ssh)).isExecutable()) return std::nullopt;
		env = QString("GIT_SSH_COMMAND=\"%1\" -i \"%2\" ").arg(QString::fromStdString(ssh)).arg(QString::fromStdString(gitinfo().ssh_key_override));
	}

	int exit_code = 0;
	GitResult result;

	auto DoIt = [&](){
		std::string cmd = fmt("\"%s\" --no-pager ")(gitCommand());

		if (opt.chdir) {
			std::string cwd = workingDir();
			if (!cwd.empty()) {
				cmd += fmt("-C \"%s\" ")(cwd);
			}
		}

		cmd += arg;

		if (opt.log) {
			std::string s = fmt("> git %s\n")(arg);
			global_write_log(s);
		}

		if (opt.pty) {
			opt.pty->start(cmd, env.toStdString(), true);
		} else {
			if (opt.use_cache) {
				auto const *a = findFromCommandCache(cmd);
				if (a) {
					result.set_output(*a);
					qDebug() << "--- Process\t" << QString::fromStdString(cmd) << "\t" << "cache hit" << "\t---";
					return;
				}
			}

			Process proc;
			proc.start(cmd, false);
			exit_code = proc.wait();

			if (opt.errout) {
				result.set_output(proc.stderr_bytes());
			} else {
				if (!proc.stderr_bytes().empty()) {
					std::vector<char> v = proc.stderr_bytes();
					std::string s = (misc::str)v;
					if (!misc::ends_with(s, '\n')) {
						s += '\n';
					}
					global_write_log(s);
				}
				result.set_output(proc.stdout_bytes());
			}
			std::vector<char> v = proc.stderr_bytes();
			std::string s(v.data(), v.size());
			result.set_error_message(s);

			if (exit_code == 0) {
				if (opt.use_cache) {
					insertIntoCommandCache(cmd, result.output());
				}
			}
		}
	};

	DoIt();
	if (exit_code == 0) {
		if (opt.pty) {
			// nop
		} else {
			result.set_exit_code(exit_code);
		}
		return result;
	} else {
		return std::nullopt;
	}
}

bool GitBasicSession::remove(const std::string &path)
{
	if ((FOUND)path.find("..")) {
		qDebug() << "Invalid path for remove: " << QString::fromStdString(path);
		return false;
	}
	QString rm_path = QString::fromStdString(workingDir() / path);
	return QFile(rm_path).remove();
}

std::optional<std::vector<GitFileItem>> GitBasicSession::ls(const char *path)
{
	std::vector<GitFileItem> ret;
	QDirIterator it(path, QDir::Files);
	while (it.hasNext()) {
		it.next();
		GitFileItem item;
		item.name = it.fileName().toStdString();
		item.isdir = it.fileInfo().isDir();
		ret.push_back(item);
	}
	return ret;
}

std::optional<std::vector<char>> GitBasicSession::readfile(const char *path)
{
	QFile file(misc::normalizePathSeparator(QString::fromUtf8(path)));
	if (!file.open(QIODevice::ReadOnly)) {
		qDebug() << "Failed to open file for reading: " << path;
		return std::nullopt;
	}
	std::vector<char> data(file.size());
	if ((size_t)file.read(data.data(), data.size()) != data.size()) {
		qDebug() << "Failed to read file: " << path;
		return std::nullopt;
	}
	return data;
}

