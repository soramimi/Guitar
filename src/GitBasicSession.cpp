#include "GitBasicSession.h"
#include "ApplicationGlobal.h"
#include "TraceEventWriter.h"
#include "common/joinpath.h"
#include "common/q/FileInfo.h"
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFileInfo>

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
		if (ssh.find('\"') != std::string::npos) return std::nullopt;
		if (gitinfo().ssh_key_override.find('\"') != std::string::npos) return std::nullopt;
		if (!QFileInfo(QString::fromStdString(ssh)).isExecutable()) return std::nullopt;
		env = QString("GIT_SSH_COMMAND=\"%1\" -i \"%2\" ").arg(QString::fromStdString(ssh)).arg(QString::fromStdString(gitinfo().ssh_key_override));
	}

	int exit_code = 0;
	GitResult result;

	auto DoIt = [&](){
		QString cmd;
#ifdef _WIN32
		cmd = opt.prefix;
#endif
		cmd += QString("\"%1\" --no-pager ").arg(QString::fromStdString(gitCommand()));

		if (opt.chdir) {
			QString cwd = QString::fromStdString(workingDir());
			if (!cwd.isEmpty()) {
				cmd += QString("-C \"%1\" ").arg(cwd);
			}
		}

		cmd += QString::fromStdString(arg);

		if (opt.log) {
			QString s = QString("> git %1\n").arg(QString::fromStdString(arg));
			global->writeLog(s);
		}

		if (opt.pty) {
			opt.pty->start(cmd.toStdString(), env.toStdString());
		} else {
			if (opt.use_cache) {
				auto const *a = findFromCommandCache(cmd);
				if (a) {
					result.set_output(*a);
					qDebug() << "--- Process\t" << cmd << "\t" << "cache hit" << "\t---";
					return;
				}
			}

			Process proc;
			proc.start(cmd.toStdString(), false);
			exit_code = proc.wait();

			if (opt.errout) {
				result.set_output(proc.errbytes);
			} else {
				if (!proc.errbytes.empty()) {
					QString s = QString::fromStdString(proc.errstring());
					if (!s.endsWith('\n')) {
						s += '\n';
					}
					global->writeLog(s);
				}
				result.set_output(proc.outbytes);
			}
			result.set_error_message(proc.errstring());

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
	if (path.find("..") != std::string::npos) {
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
	if (file.read(data.data(), data.size()) != data.size()) {
		qDebug() << "Failed to read file: " << path;
		return std::nullopt;
	}
	return data;
}

