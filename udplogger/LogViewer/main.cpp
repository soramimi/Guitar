
#include <QApplication>
#include "../RemoteLogger.h"
#include "MainWindow.h"

int main(int argc, char **argv)
{
	RemoteLogger::initialize();
	QApplication a(argc, argv);
	qRegisterMetaType<LogItem>();
	MainWindow w;
	w.show();
	int r = a.exec();
	RemoteLogger::cleanup();
	return r;
}

