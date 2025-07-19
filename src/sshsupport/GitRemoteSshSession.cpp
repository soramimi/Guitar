#include "GitRemoteSshSession.h"
#include "ApplicationGlobal.h"

GitRemoteSshSession::GitRemoteSshSession()
{
	ssh_ = std::make_shared<SshConnection>();
}

bool GitRemoteSshSession::connect(std::shared_ptr<SshConnection> ssh, std::string const &gitcmd)
{
	ready_ = false;
	ssh_ = ssh;
	git_command_ = gitcmd;

	ssh_->add_allowed_command(git_command_);
	std::string cmd = git_command_ + ' ' + "--version";
	auto ret = ssh_->exec(cmd.c_str());
	if (ret) {
		qDebug() << QString::fromStdString(*ret);
		ready_ = true;
	}
	return ready_;
}

bool GitRemoteSshSession::is_connected() const
{
	return ready_;
}

bool GitRemoteSshSession::isValidWorkingCopy(const QString &dir) const
{
	return true; // TODO
}

bool GitRemoteSshSession::exec_git(const QString &arg, const Option &opt)
{
	if (!is_connected()) return false;

	QString cmd = QString("\"%1\" --no-pager ").arg(git_command_);

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

	auto ret = ssh_->exec(cmd.toStdString().c_str());
	if (ret) {
		std::string_view result(*ret);
		var().result.clear();
		var().result.insert(var().result.end(), result.begin(), result.end());
		return true;
	}
	return false;
}

bool GitRemoteSshSession::remove(const QString &path)
{
	bool ret = false;
	if (ssh_->open_sftp()) {
		ret = ssh_->unlink(path.toStdString().c_str());
		ssh_->close_sftp();
	}
	return ret;
}

bool GitRemoteSshSession::ls(char const *path, std::vector<GitFileItem> *files)
{
	files->clear();
	if (ssh_->open_sftp()) {
		auto ret = ssh_->list(path);
		ssh_->close_sftp();
		if (ret) {
			for (SshConnection::FileItem const &item : *ret) {
				GitFileItem newitem;
				newitem.name = QString::fromStdString(item.name);
				newitem.isdir = item.isdir;
				files->push_back(newitem);
			}
			return true;
		}
	}
	return false;
}

bool GitRemoteSshSession::readfile(const char *path, std::vector<char> *data)
{
	data->clear();
	if (ssh_->open_sftp()) {
		auto ret = ssh_->pull(path);
		ssh_->close_sftp();
		if (ret) {
			data->insert(data->end(), ret->begin(), ret->end());
			return true;
		}
	}
	return false;
}
