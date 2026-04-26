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
#include <udplogger/RemoteLogger.h>
#include <QDebug>
#include "Logger.h"
#include "LibMecab.h"
#include "LibMigemo.h"

struct ApplicationGlobal::Private {
	TraceEventWriter trace_event_logger;
	RemoteLogger remote_logger;
	LibMigemo migemo; // obsolete
};

ApplicationGlobal::ApplicationGlobal()
	: m(new Private)
{
}

ApplicationGlobal::~ApplicationGlobal()
{
	close_remote_logger();
	close_trace_logger();
	delete m;
}

void ApplicationGlobal::open_remote_logger()
{
	if (appsettings.enable_remote_log) {
		std::string host = appsettings.remote_log_host.toStdString();
		int port = appsettings.remote_log_port;
		m->remote_logger.open(host.c_str(), port);
	}
}

void ApplicationGlobal::close_remote_logger()
{
	m->remote_logger.close();
}

void ApplicationGlobal::send_remote_logger(const std::string &msg, char const *file, int line)
{
	m->remote_logger.send(msg, file, line);
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
	// filetypeライブラリを初期化し、基本的な動作をテスト

	bool ok = filetype.open();
	Q_ASSERT(ok);

	{ // test "Hello, world" filetype registration
		QByteArray ba("Hello, world", 12);
		auto result = filetype.file(ba.data(), ba.size());
		if (result.mimetype != "text/plain") {
			qDebug() << "Failed to register \"Hello, world\" filetype: "
					 << QString::fromStdString(result.mimetype) << " expected text/plain";
		}
	}

	{ // test digits.png filetype registration
		QFile file(":/image/digits.png");
		file.open(QFile::ReadOnly);
		QByteArray ba = file.readAll();
		auto result = filetype.file(ba.data(), ba.size());
		if (result.mimetype != "image/png") {
			qDebug() << "Failed to register digits.png filetype: "
					 << QString::fromStdString(result.mimetype) << " expected image/png";
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

std::shared_ptr<AbstractIncrementalSearchFilter> ApplicationGlobal::makeIncrementalSearchFilter(std::string const &filtertext)
{
	if (1) {
		return std::make_shared<MecabFilter>(filtertext);
	} else { // obsolete
		return std::make_shared<MigemoFilter>(filtertext);
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

struct _AiCredentials : public GenerativeAI::AbstractVisitor<GenerativeAI::Credential> {
	char const *envname;
	_AiCredentials(char const *envname)
		: envname(envname)
	{
	}

	std::string getenv(char const *name)
	{
		char const *value = std::getenv(name);
		return value ? std::string(value) : std::string();
	}


	GenerativeAI::Credential case_Unknown()
	{
		return {};
	}

	GenerativeAI::Credential case_OpenAI_chat_completions()
	{
		GenerativeAI::Credential cred;
		if (global->appsettings.use_env_api_key_OpenAI) {
			cred.api_key = getenv(envname);
		} else {
			cred.api_key = global->appsettings.api_key_OpenAI.toStdString();
		}
		return cred;
	}

	GenerativeAI::Credential case_OpenAI_responses()
	{
		return case_OpenAI_chat_completions();
	}

	GenerativeAI::Credential case_Anthropic()
	{
		GenerativeAI::Credential cred;
		if (global->appsettings.use_env_api_key_Anthropic) {
			cred.api_key = getenv(envname);
		} else {
			cred.api_key = global->appsettings.api_key_Anthropic.toStdString();
		}
		return cred;
	}
	GenerativeAI::Credential case_Google()
	{
		GenerativeAI::Credential cred;
		if (global->appsettings.use_env_api_key_Google) {
			cred.api_key = getenv(envname);
		} else {
			cred.api_key = global->appsettings.api_key_Google.toStdString();
		}
		return cred;
	}
	GenerativeAI::Credential case_DeepSeek()
	{
		GenerativeAI::Credential cred;
		if (global->appsettings.use_env_api_key_DeepSeek) {
			cred.api_key = getenv(envname);
		} else {
			cred.api_key = global->appsettings.api_key_DeepSeek.toStdString();
		}
		return cred;
	}
	GenerativeAI::Credential case_OpenRouter()
	{
		GenerativeAI::Credential cred;
		if (global->appsettings.use_env_api_key_OpenRouter) {
			cred.api_key = getenv(envname);
		} else {
			cred.api_key = global->appsettings.api_key_OpenRouter.toStdString();
		}
		return cred;
	}

	GenerativeAI::Credential case_Ollama()
	{
		GenerativeAI::Credential cred;
		cred.api_key = "aonymous";
		return cred;
	}

	GenerativeAI::Credential case_LMStudio()
	{
		GenerativeAI::Credential cred;
		cred.api_key = "aonymous";
		return cred;
	}

	GenerativeAI::Credential case_LLAMACPP()
	{
		GenerativeAI::Credential cred;
		cred.api_key = "aonymous";
		return cred;
	}
};

GenerativeAI::Credential ApplicationGlobal::get_ai_credential(GenerativeAI::AI provider)
{
	GenerativeAI::ProviderInfo const *info = provider_info(provider);
	char const *envname = info->env_name.c_str();
	return _AiCredentials(envname).visit(provider);
}

std::string ApplicationGlobal::determineFileType(char const *data, size_t size)
{
	if (!data || size == 0) return {};

	if (size > 10) {
		if (memcmp(data, "\x1f\x8b\x08", 3) == 0) { // gzip
			QBuffer buf;
			MemoryReader mem(data, size);

			mem.open(MemoryReader::ReadOnly);
			buf.open(QBuffer::WriteOnly);
			gzip z;
			z.set_maximul_size(100000);
			SimpleQtReader reader(&mem);
			SimpleQtWriter writer(&buf);
			z.decompress(&reader, &writer);

			QByteArray uz = buf.buffer();
			if (!uz.isEmpty()) {
				return filetype.file(uz.data(), uz.size()).mimetype;
			}
		}
	}

	std::string mime = filetype.file(data, size).mimetype;
	// qDebug() << QString::fromStdString(mime);
	return mime;
}

std::string ApplicationGlobal::determineFileType(QString const &path)
{
	QFile file(path);
	if (file.open(QFile::ReadOnly)) {
		QByteArray ba = file.readAll();
		return determineFileType(ba);
	}
	return {};
}

std::string ApplicationGlobal::determineFileType(std::string const &path)
{
	return determineFileType(QString::fromStdString(path));
}

