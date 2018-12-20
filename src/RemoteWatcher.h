#ifndef REMOTEWATCHER_H
#define REMOTEWATCHER_H

#include <QThread>

#include "MainWindow.h"

class RemoteWatcher : public QThread {
	Q_OBJECT
private:
	MainWindow *mainwindow_ = nullptr;
	QString remote_;
	QString branch_;
	MainWindow *mainwindow()
	{
		return mainwindow_;
	}
public:
	RemoteWatcher() = default;
	void start(MainWindow *mw);
	void setCurrent(QString const &remote, QString const &branch);
public slots:
	void checkRemoteUpdate();
};

#endif // REMOTEWATCHER_H
