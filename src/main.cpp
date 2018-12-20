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

#include <QStandardPaths>
#include "common/joinpath.h"

ApplicationGlobal *global = nullptr;

ApplicationSettings ApplicationSettings::defaultSettings()
{
	ApplicationSettings s;
	s.proxy_server = "http://squid:3128/";
	return s;
}

static bool isHighDpiScalingEnabled()
{
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

	global->organization_name = ORGANIZATION_NAME;
	global->application_name = APPLICATION_NAME;
	global->generic_config_dir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
	global->app_config_dir = global->generic_config_dir / global->organization_name / global->application_name;
	global->config_file_path = joinpath(global->app_config_dir, global->application_name + ".ini");
	if (!QFileInfo(global->app_config_dir).isDir()) {
		QDir().mkpath(global->app_config_dir);
	}

	if (isHighDpiScalingEnabled()){
#if (QT_VERSION < QT_VERSION_CHECK(5, 6, 0))
		qDebug() << "High DPI scaling is not supported";
#else
		QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
	}

	QApplication a(argc, argv);
	QApplication::setOrganizationName(global->organization_name);
	QApplication::setApplicationName(global->application_name);

	{
		MySettings s;
		s.beginGroup("UI");
		global->language_id = s.value("Language").toString();
		global->theme_id = s.value("Theme").toString();
		if (global->theme_id.compare("dark", Qt::CaseInsensitive) == 0) {
			global->theme = createDarkTheme();
		} else {
			global->theme = createStandardTheme();
		}
		s.endGroup();
	}

	QApplication::setStyle(global->theme->newStyle());

	if (QApplication::queryKeyboardModifiers() & Qt::ShiftModifier) {
		global->start_with_shift_key = true;
	}

	WebClient::initialize();

	bool f_open_here = false;

	QStringList args;

	for (int i = 1; i < argc; i++) {
		std::string arg = argv[i];
		if (arg[0] == '-') {
			if (arg == "--open-here") {
				f_open_here = true;
			}
		} else {
			args.push_back(QString::fromStdString(arg));
		}
	}

	if (global->app_config_dir.isEmpty()) {
		QMessageBox::warning(nullptr, qApp->applicationName(), "Preparation of data storage folder failed.");
		return 1;
	}

	QTranslator translator;
	{
		if (global->language_id.isEmpty() || global->language_id == "en") {
			// thru
		} else {
#if defined(Q_OS_MACX)
			QString path = "../Resources/Guitar_" + global->language_id;
#else
			QString path = "Guitar_" + global->language_id;
#endif
			translator.load(path, QApplication::applicationDirPath());
			QApplication::installTranslator(&translator);
		}
	}

	MainWindow w;
	global->panel_bg_color = w.palette().color(QPalette::Background);
	w.setWindowIcon(QIcon(":/image/guitar.png"));
	w.show();
	w.shown();

	if (f_open_here) {
		QString dir = QDir::current().absolutePath();
		w.autoOpenRepository(dir);
	} else if (args.size() == 1) {
		QString dir = args[0] / QString();
		if (dir.startsWith("./") || dir.startsWith(".\\")) {
			dir = QDir::current().absolutePath() / dir.mid(2);
		}
		QFileInfo fi(dir);
		if (fi.isDir()) {
			dir = fi.absolutePath();
			w.autoOpenRepository(dir);
		}
	}

	return QApplication::exec();
}

