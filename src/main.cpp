#include "MainWindow.h"
#include <QApplication>
#include "ApplicationGlobal.h"
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
#include "../darktheme/src/DarkStyle.h"

ApplicationGlobal *global = 0;

ApplicationSettings ApplicationSettings::defaultSettings()
{
	ApplicationSettings s;
	s.proxy_server = "http://squid:3128/";
	return s;
}

static bool isHighDpiScalingEnabled(int argc, char *argv[])
{
	QApplication dummy(argc, argv);
	dummy.setOrganizationName(ORGANIZTION_NAME);
	dummy.setApplicationName(global->application_name);
	MySettings s;
	s.beginGroup("UI");
	QVariant v = s.value("EnableHighDpiScaling");
	return v.isNull() || v.toBool();
}

int main(int argc, char *argv[])
{
	putenv((char *)"UNICODEMAP_JP=cp932");

	ApplicationGlobal g;
	global = &g;
	global->application_name = APPLICATION_NAME;

	if (isHighDpiScalingEnabled(argc, argv)){
#if (QT_VERSION < QT_VERSION_CHECK(5, 6, 0))
		qDebug() << "High DPI scaling is not supported";
#else
		QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
	}

	QApplication a(argc, argv);
	a.setOrganizationName(ORGANIZTION_NAME);
	a.setApplicationName(global->application_name);

	{
		MySettings s;
		s.beginGroup("UI");
		QString theme = s.value("Theme").toString();
		if (theme.compare("dark", Qt::CaseInsensitive) == 0) {
			global->theme = createDarkTheme();
		} else {
			global->theme = createStandardTheme();
		}
	}

	QApplication::setStyle(global->theme->newStyle());

	if (QApplication::queryKeyboardModifiers() & Qt::ShiftModifier) {
		global->start_with_shift_key = true;
	}

	WebClient::initialize();

	bool f_open_here = false;

	for (int i = 1; i < argc; i++) {
		std::string arg = argv[i];
		if (arg == "--open-here") {
			f_open_here = true;
		}
	}

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

