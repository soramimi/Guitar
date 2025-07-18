#include "GitBasicSession.h"
#include "ApplicationGlobal.h"
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include "common/joinpath.h"
#include "common/rwfile.h"

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

bool GitBasicSession::exec_git(const QString &arg, const Option &opt)
{
	QFileInfo info2(gitCommand());
	if (!info2.isExecutable()) {
		qDebug() << "Invalid git command: " << gitCommand();
		return false;
	}

	clearResult();

	QString env;
	QString ssh = sshCommand();
	if (ssh.isEmpty() || gitinfo().ssh_key_override.isEmpty()) {
		// nop
	} else {
		if (ssh.indexOf('\"') >= 0) return false;
		if (gitinfo().ssh_key_override.indexOf('\"') >= 0) return false;
		if (!QFileInfo(ssh).isExecutable()) return false;
		env = QString("GIT_SSH_COMMAND=\"%1\" -i \"%2\" ").arg(ssh).arg(gitinfo().ssh_key_override);
	}

	auto DoIt = [&](){
		QString cmd;
#ifdef _WIN32
		cmd = opt.prefix;
#else

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
			var().exit_status.exit_code = 0; // バックグラウンドで実行を継続するけど、とりあえず成功したことにしておく
		} else {
			if (opt.use_cache) {
				auto const *a = findFromCommandCache(cmd);
				if (a) {
					var().result = *a;
					return true;
				}
			}

			Process proc;
			proc.start(cmd.toStdString(), false);
			var().exit_status.exit_code = proc.wait();

			if (opt.errout) {
				var().result = proc.errbytes;
			} else {
				if (!proc.errbytes.empty()) {
					qDebug() << QString::fromStdString(proc.errstring());
				}
				var().result = proc.outbytes;
			}
			var().exit_status.error_message = proc.errstring();

			if (var().exit_status.exit_code == 0) {
				if (opt.use_cache) {
					insertIntoCommandCache(cmd, var().result);
				}
			}
		}

		return var().exit_status.exit_code == 0;
	};

	bool ok = false;

	if (opt.pty) {
		if (opt.chdir) {
			opt.pty->setChangeDir(workingDir());
		}
		ok = DoIt();
	} else {
		ok = DoIt();
	}

	return ok;
}

// bool GitBasicSession::pushd(const std::function<bool ()> fn)
// {
// 	bool ok = false;
// 	QString cwd = QDir::currentPath();
// 	QString dir = workingDir();
// 	if (QDir::setCurrent(dir)) {

// 		ok = fn();

// 		QDir::setCurrent(cwd);
// 	}
// 	return ok;
// }

bool GitBasicSession::remove(const QString &path)
{
#if 0
	return pushd([&](){
		QFile(path).remove();
		return true;
	});
#else
	if (path.indexOf("..") >= 0) {
		qDebug() << "Invalid path for remove: " << path;
		return false;
	}
	QString rm_path = workingDir() / path;
	return QFile(rm_path).remove();
#endif
}

bool GitBasicSession::ls(const char *path, std::vector<GitFileItem> *files)
{
	files->clear();
	QDirIterator it(path, QDir::Files);
	while (it.hasNext()) {
		it.next();
		GitFileItem item;
		item.name = it.fileName();
		item.isdir = it.fileInfo().isDir();
		files->push_back(item);
	}
	return true;
}

bool GitBasicSession::readfile(const char *path, std::vector<char> *data)
{
	auto o = ::readfile(path, -1);
	if (o) {
		*data = std::move(*o);
		return true;
	}
	return false;
}

