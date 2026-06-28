
#include "ApplicationGlobal.h"
#include "ApplicationSettings.h"
#include "AvatarLoader.h"
#include "CommitLogTableWidget.h"
#include "FileType.h"
#include "GeneratedCommitMessage.h"
#include "Logger.h"
#include "MainWindow.h"
#include "MySettings.h"
#include "RepositoryModel.h"
#include "SettingGeneralForm.h"
#include "fmt.h"
#include <IncrementalSearchInterface.h>
#include <MyExperimentalLibrary.h>
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QMessageBox>
#include <QPluginLoader>
#include <QProxyStyle>
#include <QStandardPaths>
#include <QTranslator>
#include <common/joinpath.h>
#include <csignal>
#include <inet/webclient.h>
#include <string>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <dlfcn.h>
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

	s.incremental_search_color.filtered_bg = QColor(128, 128, 128, 64);
	s.incremental_search_color.highlight_bg = QColor(240, 64, 255, 128);

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

void logHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
	QString message = qFormatLogMessage(type, context, msg);
	if (message.startsWith("\"HEAD")) {
		qDebug() << message;
	}
	std::string s = message.toStdString();

	if (0) {
		fprintf(stderr, "%s\n", s.c_str());
	}

	if (1) {
		logprint(LOG_DEFAULT, s);
	}
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
	putenv(const_cast<char *>("QT_ASSUME_STDERR_HAS_CONSOLE=1"));
	if (0) {
		qInstallMessageHandler(logHandler);
	}

	WebClient::initialize();

	ApplicationGlobal g;
	global = &g;

	signal(SIGTERM, onSigTerm);
#ifndef _WIN32
	signal(SIGPIPE, onSigPipe);
#endif

	bool a_open_here = false;
	QString a_commit_id;

	QStringList args;

	{
		int i = 1;
		while (i < argc) {
			std::string arg = argv[i];
			if (arg.c_str()[0] == '-') {
				if (arg == "--open-here") {
					a_open_here = true;
				} else if (arg == "--commit-id") {
					if (i + 1 < argc) {
						i++;
						a_commit_id = argv[i];
					}
				} else if (arg == "--unsafe") { // experimental
#ifdef UNSAFE_ENABLED
					global->unsafe_enabled = true;
					qDebug() << "Unsafe mode enabled.";
#endif
				}
			} else {
				args.push_back(QString::fromStdString(arg));
			}
			i++;
		}
	}
	
#ifdef Q_OS_WIN
	putenv("QT_ENABLE_HIGHDPI_SCALING=1");
	// QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
	// qputenv("QT_SCALE_FACTOR", "1.5");
	
	QApplication a(argc, argv);
	global->organization_name = ORGANIZATION_NAME;
	global->application_name = APPLICATION_NAME;
	global->application_file_path = QCoreApplication::applicationFilePath();
	global->generic_config_dir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
	global->app_config_dir = global->generic_config_dir / global->organization_name / global->application_name;
	global->log_dir = global->app_config_dir / "log";
	global->config_file_path = joinpath(global->app_config_dir, global->application_name + ".ini");

	auto MKPATH = [&](const QString &path) {
		if (!QFileInfo(path).isDir()) {
			if (!QDir().mkpath(path)) {
				qDebug() << "Failed to create directory:" << path;
			}
		}
	};

	MKPATH(global->app_config_dir);
	MKPATH(global->log_dir);

	global->appsettings = ApplicationSettings::loadSettings();

	Logger::start();
	{
		logprintf(LOG_DEFAULT, "-----------------------------------");
		logprintf(LOG_DEFAULT, "Starting Guitar, The Git GUI Client");
		logprintf(LOG_DEFAULT, "-----------------------------------");
		logprintf(LOG_DEFAULT, "%s, v%s (%s)"
				  , APPLICATION_NAME
				  , global->product_version()
				  , global->source_revision()
				  );
	}
	Logger::pause(true);
	Logger::open(global->log_dir.toStdString() / "Guitar.log");
	Logger::pause(false);

	if (global->appsettings.enable_trace_log) {
		global->open_trace_logger();
	}

	global->profiles_xml_path = joinpath(global->app_config_dir, "profiles.xml");

	global->init1();

	// load incremental search plugin
	{
		QString name = "incrementalsearchplugin";
#ifdef QT_DEBUG
		name += 'd';
#endif
#ifdef Q_OS_WIN
		QString path = name;
#else
		QString path = QCoreApplication::applicationDirPath() / "../lib/lib" + name + ".so";
#endif
		logprintf(LOG_DEFAULT, "loading the plugin %s", path.toStdString().c_str());
		QPluginLoader loader(path);
		IncrementalSearchInterface *plugin = dynamic_cast<IncrementalSearchInterface *>(loader.instance());
		if (plugin) {
			global->incremental_search = std::shared_ptr<IncrementalSearch>(plugin->create());
			if (global->incremental_search->open()) {
				logprintf(LOG_DEFAULT, "%s is loaded successfully.", name.toStdString().c_str());
			} else {
				logprintf(LOG_DEFAULT, "%s is loaded but failed to open. This may cause some features to not work properly.", name.toStdString().c_str());
			}
		} else {
			logprint(LOG_DEFAULT, loader.errorString().toStdString().c_str());
		}
	}

	// experimental shared library loading test
#ifdef EXPERIMENTAL_FILETYPEPLUGIN
	if (1) {
		MyExperimentalLibrary lib;
		if (lib.open()) {
			// lib.hoge();
		}
	}
#endif

	global->init2();

	QApplication::setOrganizationName(global->organization_name);
	QApplication::setApplicationName(global->application_name);

	qRegisterMetaType<AvatarLoaderItem>("AvatarLoaderItem");
	qRegisterMetaType<RepositoryInfo>("RepositoryInfo");
	qRegisterMetaType<GeneratedCommitMessage>("GeneratedCommitMessage");
	qRegisterMetaType<MainWindowExchangeData>("MainWindowExchangeData");
	qRegisterMetaType<GitCommandRunner>("GitCommandRunner");
	qRegisterMetaType<PtyProcessCompleted>("PtyProcessCompleted");
	qRegisterMetaType<CloneParams>("CloneParams");
	qRegisterMetaType<LogData>("LogData");
	qRegisterMetaType<CommitLogExchangeData>("CommitLogExchangeData");
	qRegisterMetaType<CommitRecord>("CommitRecord");
	qRegisterMetaType<StatusInfo>("StatusInfo");

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

	WebClient::cleanup();
	Logger::stop();
	return r;
}

