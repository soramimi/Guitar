
#include "main.h"
#include "ApplicationGlobal.h"
#include "AvatarLoader.h"
#include "MainWindow.h"
#include "MySettings.h"
#include "SettingGeneralForm.h"
#include "common/joinpath.h"
#include "common/misc.h"
#include "darktheme/DarkStyle.h"
#include "platform.h"
#include "webclient.h"
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QMessageBox>
#include <QProxyStyle>
#include <QStandardPaths>
#include <QTranslator>
#include <csignal>
#include <string>

#ifdef Q_OS_WIN
#include "win32/win32.h"

#include "SettingGeneralForm.h"
#endif

#ifndef APP_GUITAR
#error APP_GUITAR is not defined.
#endif

ApplicationGlobal *global = nullptr;

ApplicationSettings ApplicationSettings::defaultSettings()
{
	ApplicationSettings s;
	s.proxy_server = "http://squid:3128/";

	s.branch_label_color.head = QColor(255, 192, 224); // pink
	s.branch_label_color.local = QColor(192, 224, 255); // blue
	s.branch_label_color.remote = QColor(192, 240, 224); // green
	s.branch_label_color.tag = QColor(255, 224, 192); // orange

#ifdef _WIN32
	s.terminal_command = "cmd"; // or wsl
#else
#ifdef __HAIKU__
	s.terminal_command = "Terminal";
#else
	s.terminal_command = "x-terminal-emulator";
#endif
#endif

	return s;
}

void setEnvironmentVariable(QString const &name, QString const &value);

void onSigTerm(int)
{
	qDebug() << "SIGTERM caught";
	if (global->mainwindow) {
		global->mainwindow->close();
	}
}

void onSigPipe(int)
{
	qDebug() << "SIGPIPE caught";
	global->webcx.notify_broken_pipe();
}

int main(int argc, char *argv[])
{
	putenv("QT_ASSUME_STDERR_HAS_CONSOLE=1");

	ApplicationGlobal g;
	global = &g;
	signal(SIGTERM, onSigTerm);
#ifndef _WIN32
	signal(SIGPIPE, onSigPipe);
#endif

	global->organization_name = ORGANIZATION_NAME;
	global->application_name = APPLICATION_NAME;
	global->this_executive_program = QFileInfo(argv[0]).absoluteFilePath();
	global->generic_config_dir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
	global->app_config_dir = global->generic_config_dir / global->organization_name / global->application_name;
	global->config_file_path = joinpath(global->app_config_dir, global->application_name + ".ini");
	global->profiles_xml_path = joinpath(global->app_config_dir, "profiles.xml");
	if (!QFileInfo(global->app_config_dir).isDir()) {
		QDir().mkpath(global->app_config_dir);
	}

#ifdef Q_OS_WIN
	putenv("QT_ENABLE_HIGHDPI_SCALING=1");
	// QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
	// qputenv("QT_SCALE_FACTOR", "1.5");


	QApplication a(argc, argv);

	global->init(&a);

	QApplication::setOrganizationName(global->organization_name);
	QApplication::setApplicationName(global->application_name);

	qRegisterMetaType<AvatarLoader::Item>("AvatarLoader::Item");
	qRegisterMetaType<RepositoryData>("RepositoryData");
	qRegisterMetaType<RepositoryWrapperFrameP>("RepositoryWrapperFrameP");

	{
		MySettings s;
		s.beginGroup("UI");
		global->language_id = s.value("Language").toString();
		global->theme_id = s.value("Theme").toString();
		if (global->theme_id.compare("dark", Qt::CaseInsensitive) == 0) {
			global->theme = createDarkTheme();
			a.setStyle(global->theme->newStyle());
			a.setPalette(a.style()->standardPalette());
		} else {
			global->theme = createLightTheme();
			a.setStyle(global->theme->newStyle());
#ifndef Q_OS_WIN
			a.setPalette(a.style()->standardPalette());
#endif
		}
		s.endGroup();
	}

	if (QApplication::queryKeyboardModifiers() & Qt::ShiftModifier) {
		global->start_with_shift_key = true;
	}

	WebClient::initialize();

	bool a_open_here = false;
	QString a_commit_id;

	QStringList args;

	{
		int i = 1;
		while (i < argc) {
			std::string arg = argv[i];
			if (arg[0] == '-') {
				if (arg == "--open-here") {
					a_open_here = true;
				} else if (arg == "--commit-id") {
					if (i + 1 < argc) {
						i++;
						a_commit_id = argv[i];
					}
				}
			} else {
				args.push_back(QString::fromStdString(arg));
			}
			i++;
		}
	}

	if (global->app_config_dir.isEmpty()) {
		QMessageBox::warning(nullptr, qApp->applicationName(), "Preparation of data storage folder failed.");
		return 1;
	}

	// 設定ファイルがないときは、言語の選択をする。
	if (!QFileInfo::exists(global->config_file_path)) {
		auto langs = SettingGeneralForm::languages();
		SettingGeneralForm::execSelectLanguageDialog(nullptr, langs, [](){});
	}

	QTranslator translator;
	{
		if (global->language_id.isEmpty() || global->language_id == "en") {
			// thru
		} else {
			QString path = ":/translations/Guitar_" + global->language_id;
			bool f = translator.load(path, QApplication::applicationDirPath());
			if (!f) {
				qDebug() << QString("Failed to load the translation file: %1").arg(path);
			}
			QApplication::installTranslator(&translator);
		}
	}

	MainWindow w;
	global->mainwindow = &w;
	global->panel_bg_color = w.palette().color(QPalette::Window);
	w.setWindowIcon(QIcon(":/image/guitar.png"));
	w.show();
	w.shown();

	if (a_open_here) {
		QString dir = QDir::current().absolutePath();
		w.autoOpenRepository(dir, a_commit_id);
	} else if (args.size() == 1) {
		QString dir = args[0] / QString();
		if (dir.startsWith("./") || dir.startsWith(".\\")) {
			dir = QDir::current().absolutePath() / dir.mid(2);
		}
		QFileInfo fi(dir);
		if (fi.isDir()) {
			dir = fi.absolutePath();
			w.autoOpenRepository(dir, a_commit_id);
		}
	}

	global->webcx.set_keep_alive_enabled(true);
	global->avatar_loader.start(&w);

	int r = QApplication::exec();

	global->avatar_loader.stop();

	global->mainwindow = nullptr;
	return r;
}

