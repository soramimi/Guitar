#include "GitBasicSession.h"
#include "ApplicationGlobal.h"
#include "common/joinpath.h"
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

QString GitBasicSession::gitCommand() const
{
	return gitinfo().git_command;
}

QString GitBasicSession::sshCommand() const
{
	return gitinfo().ssh_command;
}

bool GitBasicSession::is_connected() const
{
	return QFileInfo(gitCommand()).isExecutable();
}

std::optional<GitResult> GitBasicSession::exec_git(const QString &arg, const Option &opt)
{
	QFileInfo info2(gitCommand());
	if (!info2.isExecutable()) {
		qDebug() << "Invalid git command: " << gitCommand();
		return std::nullopt;
	}

	QString env;
	QString ssh = sshCommand();
	if (ssh.isEmpty() || gitinfo().ssh_key_override.isEmpty()) {
		// nop
	} else {
		if (ssh.indexOf('\"') >= 0) return std::nullopt;
		if (gitinfo().ssh_key_override.indexOf('\"') >= 0) return std::nullopt;
		if (!QFileInfo(ssh).isExecutable()) return std::nullopt;
		env = QString("GIT_SSH_COMMAND=\"%1\" -i \"%2\" ").arg(ssh).arg(gitinfo().ssh_key_override);
	}

	int exit_code = 0;
	GitResult result;

	auto DoIt = [&](){
		QString cmd;
#ifdef _WIN32
		cmd = opt.prefix;
#endif
		cmd += QString("\"%1\" --no-pager ").arg(gitCommand());

		if (opt.chdir) {
			QString cwd = workingDir();
			if (!cwd.isEmpty()) {
				cmd += QString("-C \"%1\" ").arg(cwd);
			}
		}

		cmd += arg;

		if (opt.log) {
			QString s = QString("> git %1\n").arg(arg);
			global->writeLog(s);
		}

		if (opt.pty) {
			opt.pty->start(cmd, env);
		} else {
			if (opt.use_cache) {
				auto const *a = findFromCommandCache(cmd);
				if (a) {
					result.set_output(*a);
					qDebug() << "--- Process\t" << cmd << "\t" << "cache hit" << "\t---";
					return;
				}
			}

			QElapsedTimer timer;
			timer.start();

			Process proc;
			proc.start(cmd.toStdString(), false);
			exit_code = proc.wait();

			qDebug() << "--- Process\t" << cmd << "\t" << timer.elapsed() << "\t---";

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

bool GitBasicSession::remove(const QString &path)
{
	if (path.indexOf("..") >= 0) {
		qDebug() << "Invalid path for remove: " << path;
		return false;
	}
	QString rm_path = workingDir() / path;
	return QFile(rm_path).remove();
}

std::optional<std::vector<GitFileItem>> GitBasicSession::ls(const char *path)
{
	std::vector<GitFileItem> ret;
	QDirIterator it(path, QDir::Files);
	while (it.hasNext()) {
		it.next();
		GitFileItem item;
		item.name = it.fileName();
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

