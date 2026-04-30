#include "ApplicationGlobal.h"
#include "IncrementalSearch.h"
#include "MainWindow.h"
#include "MemoryReader.h"
#include "SimpleQtIO.h"
#include "TraceEventWriter.h"
#include "gzip.h"
#include <QBuffer>
#include <QFileIconProvider>
#include <memory>
#include <QDebug>
#include "Logger.h"
#include "MyMecab.h"
#include "LibMigemo.h"

struct ApplicationGlobal::Private {
	TraceEventWriter trace_event_logger;
	LibMigemo migemo; // obsolete
};

ApplicationGlobal::ApplicationGlobal()
	: m(new Private)
{
}

ApplicationGlobal::~ApplicationGlobal()
{
	close_trace_logger();
	delete m;
}

void ApplicationGlobal::open_trace_logger()
{
	if (global->appsettings.enable_trace_log) {
		QString dir;
		if (global->appsettings.use_custom_log_dir) {
			dir = global->appsettings.custom_log_dir;
		} else {
			dir = global->log_dir;
		}
		m->trace_event_logger.open(dir);
	}
}

void ApplicationGlobal::close_trace_logger()
{
	m->trace_event_logger.close();
}

void ApplicationGlobal::restart_trace_logger()
{
	close_trace_logger();
	open_trace_logger();
}

void ApplicationGlobal::put_trace_event(TraceEventWriter::Event const &event)
{
	m->trace_event_logger.put(event);
}

GitContext ApplicationGlobal::gcx()
{
	GitContext gcx;
	gcx.git_command = appsettings.git_command.toStdString();
	gcx.ssh_command = appsettings.ssh_command.toStdString();
	return gcx;
}



// QApplicationが構築される前に実行する
void ApplicationGlobal::init1()
{
	// filetypeライブラリの基本的な動作をテスト

	{ // test "Hello, world" filetype registration
		QByteArray ba("Hello, world", 12);
		auto mime = mimetype_by_data(ba.data(), ba.size());
		if (mime != "text/plain") {
			qDebug() << "Failed to register \"Hello, world\" filetype: "
					 << QString::fromStdString(mime) << " expected text/plain";
		}
	}

	{ // test digits.png filetype registration
		QFile file(":/image/digits.png");
		file.open(QFile::ReadOnly);
		QByteArray ba = file.readAll();
		auto mime = mimetype_by_data(ba.data(), ba.size());
		if (mime != "image/png") {
			qDebug() << "Failed to register digits.png filetype: "
					 << QString::fromStdString(mime) << " expected image/png";
		}
	}
}

// QApplicationが構築された後に実行する
void ApplicationGlobal::init2()
{
	// インクリメンタル検索ライブラリを初期化

	{ // mecab
		std::string s = mecab.convert_roman_to_katakana("wagahaihanekodearu");
		if (s != "ワガハイハネコデアル") {
			qDebug() << "Failed to convert romaji to katakana: " << QString::fromStdString(s);
		}
		if (!mecab.open("/dummy")) {
			qDebug() << "Failed to load dictionary for MeCaSearch. This may cause some features to not work properly.";
		}
		auto parts = mecab.parse("吾輩は猫である");
		if (parts.size() != 5) {
			qDebug() << "Failed to parse sentence with MeCaSearch. Expected 5 parts, got " << parts.size();
		} else {
			std::vector<std::string> expected = {"ワガハイ", "ハ", "ネコ", "デ", "アル"};
			for (size_t i = 0; i < parts.size(); i++) {
				if (parts[i].text != expected[i]) {
					qDebug() << "Failed to parse sentence with MeCaSearch. Part " << i << ": expected \"" << QString::fromStdString(expected[i]) << "\", got \"" << QString::fromStdString(parts[i].text) << "\"";
				}
			}
		}
	}

	{ // migemo // obsolete
		m->migemo.init();
		m->migemo.open();
	}

	// グローバル画像リソースの読み込み

	graphics = std::make_unique<Graphics>();
	{ // load graphic resources
		QFileIconProvider icons;
		graphics->folder_icon = icons.icon(QFileIconProvider::Folder);
		graphics->repository_icon = QIcon(":/image/repository.png");
		graphics->signature_good_icon = QIcon(":/image/signature-good.png");
		graphics->signature_bad_icon = QIcon(":/image/signature-bad.png");
		graphics->signature_dubious_icon = QIcon(":/image/signature-dubious.png");
		graphics->transparent_pixmap = QPixmap(":/image/transparent.png");
		graphics->small_digits.load(":/image/digits.png");
	}
}

void ApplicationGlobal::writeLog(const std::string_view &str)
{
	if (!mainwindow) return;
	mainwindow->emitWriteLog(str);
}

void ApplicationGlobal::writeLog(const QString &str)
{
	if (!mainwindow) return;
	mainwindow->emitWriteLog(str);
}

std::shared_ptr<AbstractInetClient> ApplicationGlobal::inet_client()
{
	std::shared_ptr<AbstractInetClient> http;
#ifdef USE_LIBCURL
	http = std::make_shared<CurlClient>(&global->curlcx);
#else
	http = std::make_shared<WebClient>(&global->webcx);
#endif
	return http;
}

LibMigemo *ApplicationGlobal::incremental_search()
{
	return &m->migemo;
}

IncrementalSearchFilter ApplicationGlobal::makeIncrementalSearchFilter(std::string const &filtertext)
{
	if (1) {
		return {std::make_shared<MecabFilter>(filtertext)};
	} else { // obsolete
		return {std::make_shared<MigemoFilter>(filtertext)};
	}
}

bool ApplicationGlobal::isMainThread()
{
	return QThread::currentThread() == QCoreApplication::instance()->thread();
}

void GlobalSetOverrideWaitCursor()
{
	QApplication::setOverrideCursor(Qt::WaitCursor);
}

void GlobalRestoreOverrideCursor()
{
	QApplication::restoreOverrideCursor();
}

GenerativeAI::Credential ApplicationGlobal::get_ai_credential(GenerativeAI::AI aiid)
{
	GenerativeAI::Credential cred;
	ApplicationSettings::AiApiKey *apikey = nullptr;
	GenerativeAI::ProviderInfo const *provider = GenerativeAI::provider_info(aiid); // 絶対に非nullptrを返す
	Q_ASSERT(provider);
	auto it = global->appsettings.ai_api_keys.find(provider->env_name);
	if (it != global->appsettings.ai_api_keys.end()) {
		apikey = &it->second;
	}
	if (apikey && apikey->from == ApplicationSettings::ApiKeyFrom::UserInput) {
		if (apikey) {
			cred.api_key = misc::trimmed(apikey->api_key);
		}
	} else if (!provider->env_name.empty()) {
		char const *env = std::getenv(provider->env_name.c_str());
		if (env) {
			cred.api_key = env;
		}
	}
	// cred.api_key = "aonymous";
	return cred;
}

std::string ApplicationGlobal::mimetype_by_data(char const *data, size_t size)
{
	return file_type_detector.mimetype_by_data(data, size);
}

std::string ApplicationGlobal::mimetype_by_data(QByteArray const &ba)
{
	return file_type_detector.mimetype_by_data(ba.data(), ba.size());
}

std::string ApplicationGlobal::mimetype_by_data(std::vector<char> const &ba)
{
	return file_type_detector.mimetype_by_data(ba.data(), ba.size());
}

std::string ApplicationGlobal::mimetype_by_file(char const *path)
{
	return file_type_detector.mimetype_by_file(path);
}

std::string ApplicationGlobal::mimetype_by_file(const std::string &path)
{
	return file_type_detector.mimetype_by_file(path);
}

