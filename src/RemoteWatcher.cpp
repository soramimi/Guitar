#include "RemoteWatcher.h"
#include "common/joinpath.h"
#include <QFileInfo>

RemoteWatcher::RemoteWatcher()
{
}

void RemoteWatcher::start(MainWindow *mw)
{
	mainwindow_ = mw;
	QThread::start();
}

void RemoteWatcher::setCurrent(const QString &remote, const QString &branch)
{
	remote_ = remote;
	branch_ = branch;
}

void RemoteWatcher::checkRemoteUpdate()
{
	QString dir = mainwindow()->currentWorkingCopyDir();

	QString refs;
	QString local_id;

	if (QFileInfo(dir).isDir()) {
		refs = "refs/remotes" / remote_ / branch_;
		QString head = dir / ".git" / refs;
		QFile file1(head);
		if (file1.open(QFile::ReadOnly)) {
			local_id = file1.readLine().trimmed();
		}
		refs = "refs/heads" / branch_;
	}
	if (local_id.isEmpty()) return;

	QString remote_id;
	GitPtr g = mainwindow()->git();
	auto list = g->ls_remote();
	for (auto item : list) {
		if (item.name == refs) {
			remote_id = item.commit_id;
			break;
		}
	}
	mainwindow()->setRemoteChanged(remote_id != local_id);
}

