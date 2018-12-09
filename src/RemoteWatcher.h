#ifndef REMOTEWATCHER_H
#define REMOTEWATCHER_H

#include <QThread>

#include "MainWindow.h"

class RemoteWatcher : public QObject {
	Q_OBJECT
private:
	MainWindow *mainwindow_ = nullptr;
	MainWindow *mainwindow()
	{
		return mainwindow_;
	}
public:
	RemoteWatcher();
	void setup(MainWindow *mw);
public slots:
	void checkRemoteUpdate();
};

#endif // REMOTEWATCHER_H
