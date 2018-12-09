#include "RemoteWatcher.h"
#include "common/joinpath.h"
#include <QFileInfo>

RemoteWatcher::RemoteWatcher()
{
}

void RemoteWatcher::setup(MainWindow *mw)
{
	mainwindow_ = mw;
}

void RemoteWatcher::checkRemoteUpdate()
{
	QString dir = mainwindow()->currentWorkingCopyDir();

	QString refs;
	QString local_id;

	if (QFileInfo(dir).isDir()) {
		QString head = dir / ".git/refs/remotes/origin/HEAD";
		QFile file1(head);
		if (file1.open(QFile::ReadOnly)) {
			refs = file1.readLine().trimmed();
			if (refs.startsWith("ref: ")) {
				refs = refs.mid(5);
				QString dir2 = dir / ".git" / refs;
				QFile file2(dir2);
				if (file2.open(QFile::ReadOnly)) {
					local_id = file2.readLine().trimmed();
				}
			}
		}
	}
	if (local_id.isEmpty()) return;

	if (refs.startsWith("refs/remotes/")) {
		QStringList v = refs.split('/');
		if (v.size() < 3) return;
		v.erase(v.begin() + 2);
		v[1] = "heads";
		refs = QString();
		for (int i = 0; i < v.size(); i++) {
			if (i > 0) {
				refs += '/';
			}
			refs += v[i];
		}
	} else {
		return;
	}

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

