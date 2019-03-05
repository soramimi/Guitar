#include "RemoteWatcher.h"
#include "common/joinpath.h"
#include <QFileInfo>

void RemoteWatcher::start(MainWindow *mw)
{
	mainwindow_ = mw;
	QThread::start();
	moveToThread(this);
}

void RemoteWatcher::setCurrent(QString const &remote, QString const &branch)
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
	for (auto const &item : list) {
		if (item.name == refs) {
			remote_id = item.commit_id;
			break;
		}
	}

	bool changed = remote_id != local_id;
	mainwindow()->notifyRemoteChanged(changed);
}

