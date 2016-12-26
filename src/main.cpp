#include "MainWindow.h"
#include <QApplication>
#include "MySettings.h"
#include "main.h"
#include <string>
#include <QMessageBox>
#include <QDir>
#include <QDebug>

QString application_data_dir;


void test();

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);

	bool f_open_here = false;

	for (int i = 1; i < argc; i++) {
		std::string arg = argv[i];
		if (arg == "--open-here") {
			f_open_here = true;
		}
	}

	a.setOrganizationName(ORGANIZTION_NAME);
	a.setApplicationName(APPLICATION_NAME);

	application_data_dir = makeApplicationDataDir();
	if (application_data_dir.isEmpty()) {
		QMessageBox::warning(0, qApp->applicationName(), "Preparation of data storage folder failed.");
		return 1;
	}

	MainWindow w;
	w.setWindowIcon(QIcon(":/image/guitar.png"));
	w.show();

	if (f_open_here) {
		QString dir = QDir::current().absolutePath();
		w.autoOpenRepository(dir);
	}

	return a.exec();
}
