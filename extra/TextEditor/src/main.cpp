#include "MainWindow.h"
#include <QApplication>
#include <QDebug>

QString makeApplicationDataDir();

int main(int argc, char *argv[])
{
	putenv("QT_ASSUME_STDERR_HAS_CONSOLE=1");

	QApplication a(argc, argv);

	a.setOrganizationName("soramimi.jp");
	a.setApplicationName("ore");

	MainWindow w;
	w.show();

	return a.exec();
}
