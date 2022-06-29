
#include "main.h"
#include "ApplicationGlobal.h"
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

static bool isHighDpiScalingEnabled()
{
	MySettings s;
	s.beginGroup("UI");
	QVariant v = s.value("EnableHighDpiScaling");
	return v.isNull() || v.toBool();
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

class QMessageLogContext;

class DebugMessageHandler {
public:
	DebugMessageHandler() = delete;

	static void install();

	static void abort(const QMessageLogContext &context, const QString &message);
};

void DebugMessageHandler::abort(QMessageLogContext const &context, const QString &message)
{
	::abort();
}

void debugMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
	(QTextStream(stderr) << qFormatLogMessage(type, context, message) << '\n').flush();

	if (type == QtFatalMsg) {
		DebugMessageHandler::abort(context, message);
	}
}

void DebugMessageHandler::install()
{
	qInstallMessageHandler(debugMessageHandler);
}

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
	setEnvironmentVariable("UNICODEMAP_JP", "cp932");
#else
	setenv("UNICODEMAP_JP", "cp932", 1);
#endif

	DebugMessageHandler::install();

	ApplicationGlobal g;
	global = &g;
	signal(SIGTERM, onSigTerm);
	signal(SIGPIPE, onSigPipe);

	global->organization_name = ORGANIZATION_NAME;
	global->application_name = APPLICATION_NAME;
	global->generic_config_dir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
	global->app_config_dir = global->generic_config_dir / global->organization_name / global->application_name;
	global->config_file_path = joinpath(global->app_config_dir, global->application_name + ".ini");
	if (!QFileInfo(global->app_config_dir).isDir()) {
		QDir().mkpath(global->app_config_dir);
	}

#if (QT_VERSION < QT_VERSION_CHECK(5, 6, 0))
	if (isHighDpiScalingEnabled()){
		qDebug() << "High DPI scaling is not supported";
	}
#else
	if (isHighDpiScalingEnabled()){
		QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);
		QCoreApplication::setAttribute(Qt::AA_DisableHighDpiScaling, false);
	} else {
		QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling, false);
		QCoreApplication::setAttribute(Qt::AA_DisableHighDpiScaling, true);
	}
#endif
//	qputenv("QT_SCALE_FACTOR", "1.5");

	QApplication a(argc, argv);
	a.setAttribute(Qt::AA_UseHighDpiPixmaps);

	global->init(&a);

	QApplication::setOrganizationName(global->organization_name);
	QApplication::setApplicationName(global->application_name);

	qRegisterMetaType<RepositoryData>("RepositoryData");
	qRegisterMetaType<RepositoryWrapperFrameP>("RepositoryWrapperFrameP");

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

	a.setStyle(global->theme->newStyle());
	a.setPalette(a.style()->standardPalette());

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

	global->webcx.set_keep_alive_enabled(true);
	global->avatar_loader.start(&w);

	int r = QApplication::exec();

	global->avatar_loader.stop();

	global->mainwindow = nullptr;
	return r;
}

