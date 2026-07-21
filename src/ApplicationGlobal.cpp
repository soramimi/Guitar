#include "IncrementalSearchHelper.h"
#include "Logger.h"
#include "MainWindow.h"
#include "MemoryReader.h"
#include "SimpleQtIO.h"
#include "TraceEventWriter.h"
#include "ai/GenerativeAI.h"
#include "gzip.h"
#include "main.h"
#include <QApplication>
#include <QBuffer>
#include <QDebug>
#include <QFileIconProvider>
#include <QListWidgetItem>
#include <QThread>
#include <common/misc.h>
#include <memory>

namespace ver {
#include "../version.c"

#include "TraceEventItem.h"
}

struct ApplicationGlobal::Private {
	TraceEventWriter trace_event_logger;
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

int ApplicationGlobal::copyright_year()
{
	return ver::copyright_year;
}

const char *ApplicationGlobal::product_version()
{
	return ver::product_version;
}

const char *ApplicationGlobal::source_revision()
{
	return ver::source_revision;
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

void ApplicationGlobal::put_trace_event(TraceEventItem const &event)
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

// QApplicationが構築された後に実行する
void ApplicationGlobal::selftest()
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
		(void)file.open(QFile::ReadOnly);
		QByteArray ba = file.readAll();
		auto mime = mimetype_by_data(ba.data(), ba.size());
		if (mime != "image/png") {
			qDebug() << "Failed to register digits.png filetype: "
					 << QString::fromStdString(mime) << " expected image/png";
		}
	}
	
	// インクリメンタル検索ライブラリを初期化

	if (!global->incremental_search) {
		logprintf(LOG_DEFAULT, "IncrementalSearchPlugin is not loaded. This may cause some features to not work properly.");
	} else {
		try {
			if (0) { // main関数でプラグインをロードしたときにopenしているはずなので、ここではopenしない。
				if (!global->incremental_search->open()) {
					throw QString("Failed to load dictionary for IncrementalSearchPlugin. This may cause some features to not work properly.");
				}
			}
			std::string s = global->incremental_search->convertRomajiToKatakana("wagahaihanekodearu");
			if (s != "ワガハイハネコデアル") {
				throw QString("Failed to convert romaji to katakana: ") + QString::fromStdString(s);
			}
			IncrementalSearchFilter filter = global->incremental_search->makeFilter("neko");
			incrementalsearch::Result r = global->incremental_search->match("吾輩は猫である", filter);
			if (!r) {
				throw QString("Failed to match romaji filter with IncrementalSearchPlugin. Expected match, got no match.");
			}
			if (!(r.parts.size() == 3)) {
				throw QString("Failed to match romaji filter with IncrementalSearchPlugin. Expected 3 parts, got " + QString::number(r.parts.size()));
			}
			if (!(r.parts[0].text == "吾輩は" && !r.parts[0].match)) {
				throw QString("Failed to match romaji filter with IncrementalSearchPlugin. Expected part 0 to be \"吾輩は\", got \"" + QString::fromStdString(r.parts[0].text) + "\"");
			}
			if (!(r.parts[1].text == "猫" && r.parts[1].match)) {
				throw QString("Failed to match romaji filter with IncrementalSearchPlugin. Expected part 1 to be \"猫\", got \"" + QString::fromStdString(r.parts[1].text) + "\"");
			}
			if (!(r.parts[2].text == "である" && !r.parts[2].match)) {
				throw QString("Failed to match romaji filter with IncrementalSearchPlugin. Expected part 2 to be \"である\", got \"" + QString::fromStdString(r.parts[2].text) + "\"");
			}
		} catch (QString const &e) {
			qDebug() << e;
		}
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
	mainwindow->emitWriteLog(str, LogChannel::Default);
}

void ApplicationGlobal::writeLog(const QString &str)
{
	if (!mainwindow) return;
	mainwindow->emitWriteLog(str, LogChannel::Default);
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

IncrementalSearchFilter ApplicationGlobal::makeIncrementalSearchFilter(std::string const &filtertext)
{
	if (!incremental_search) {
		// maybe the plugin is not loaded
		return {};
	}
	return incremental_search->makeFilter(filtertext);
}

bool ApplicationGlobal::isMainThread()
{
	return QThread::currentThread() == QCoreApplication::instance()->thread();
}

QListWidgetItem *new_QListWidgetItem(QString const &text)
{
	auto *p = new QListWidgetItem(text);
	p->setSizeHint({20, 20});
	return p;
}

void GlobalSetOverrideWaitCursor()
{
	QApplication::setOverrideCursor(Qt::WaitCursor);
}

void GlobalRestoreOverrideCursor()
{
	QApplication::restoreOverrideCursor();
}

GenerativeAI::Credential ApplicationGlobal::get_ai_credential(GenerativeAI::Model const &model)
{
	GenerativeAI::Credential cred;
	AiApiKeys::Item *apikey = nullptr;
	GenerativeAI::ProviderInfo const *provider = GenerativeAI::provider_info(model.provider_id()); // 絶対に非nullptrを返す
	Q_ASSERT(provider);
	std::string envname = provider->env_name;
	if (envname.empty()) return {};
	// if (envname.empty()) {
	// 	envname = GenerativeAI::makeEnvName(model.model_uri());
	// }
	auto it = global->appsettings.ai_api_keys.map.find(envname);
	if (it != global->appsettings.ai_api_keys.map.end()) {
		apikey = &it->second;
	}
	if (apikey && apikey->from == AiApiKeys::KeyFrom::LocalSecret) {
		if (apikey) {
			cred.api_key = misc::trimmed(apikey->api_key);
		}
	} else if (!envname.empty()) {
		char const *env = std::getenv(envname.c_str());
		if (env) {
			cred.api_key = env;
		}
	}
	// cred.api_key = "aonymous";
	return cred;
}

std::string ApplicationGlobal::mimetype_by_data(char const *data, size_t size)
{
	return file_type_detector->detect(data, size).mimetype;
}

std::string ApplicationGlobal::mimetype_by_data(QByteArray const &ba)
{
	return mimetype_by_data(ba.data(), ba.size());
}

std::string ApplicationGlobal::mimetype_by_data(std::vector<char> const &ba)
{
	return mimetype_by_data(ba.data(), ba.size());
}

std::string ApplicationGlobal::mimetype_by_file(char const *path)
{
	return file_type_detector->detect(path).mimetype;
}

std::string ApplicationGlobal::mimetype_by_file(const std::string &path)
{
	return mimetype_by_file(path.c_str());
}


QString AccountProfile::text() const
{
	return QString("%1 <%2>").arg(name).arg(email);
}

AccountProfile::operator bool() const
{
	return misc::isValidMailAddress(email.toStdString());
}
