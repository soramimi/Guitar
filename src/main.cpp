#include "MainWindow.h"
#include <QApplication>
#include "ApplicationGlobal.h"
#include "DarkStyle.h"
#include "MySettings.h"
#include "main.h"
#include <string>
#include <QMessageBox>
#include <QDir>
#include <QDebug>
#include <QProxyStyle>
#include <QTranslator>
#include "webclient.h"
#include "win32/win32.h"
#include "common/misc.h"

ApplicationGlobal *global = 0;

int main(int argc, char *argv[])
{
	ApplicationGlobal g;
	global = &g;

	global->theme = createStandardTheme();
//	global->theme = createDarkTheme();

	QApplication a(argc, argv);
	QApplication::setStyle(global->theme->newStyle());

	WebClient::initialize();

	bool f_open_here = false;

	for (int i = 1; i < argc; i++) {
		std::string arg = argv[i];
		if (arg == "--open-here") {
			f_open_here = true;
		}
	}

	a.setOrganizationName(ORGANIZTION_NAME);
	a.setApplicationName(APPLICATION_NAME);

	global->application_data_dir = makeApplicationDataDir();
	if (global->application_data_dir.isEmpty()) {
		QMessageBox::warning(0, qApp->applicationName(), "Preparation of data storage folder failed.");
		return 1;
	}

	QTranslator translator;
	{
#if defined(Q_OS_MACX)
		QString path = "../Resources/Guitar_ja";
#else
		QString path = "Guitar_ja";
#endif
		translator.load(path, a.applicationDirPath());
		a.installTranslator(&translator);
	}

	MainWindow w;
	global->panel_bg_color = w.palette().color(QPalette::Background);
	w.setWindowIcon(QIcon(":/image/guitar.png"));
	w.show();
	w.shown();

	if (f_open_here) {
		QString dir = QDir::current().absolutePath();
		w.autoOpenRepository(dir);
	}

	return a.exec();
}
