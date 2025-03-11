#include "ApplicationGlobal.h"
#include "MainWindow.h"
#include <memory>
#include <QFileIconProvider>
#include "IncrementalSearch.h"

struct ApplicationGlobal::Private {
	IncrementalSearch incremental_search;
};

ApplicationGlobal::ApplicationGlobal()
	: m(new Private)
{
}

ApplicationGlobal::~ApplicationGlobal()
{
	delete m;
}

Git::Context ApplicationGlobal::gcx()
{
	Git::Context gcx;
	gcx.git_command = appsettings.git_command;
	gcx.ssh_command = appsettings.ssh_command;
	return gcx;
}

void ApplicationGlobal::init(QApplication *a)
{
	(void)a;
	filetype.open();

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

	m->incremental_search.init();
}

void ApplicationGlobal::writeLog(const std::string_view &str)
{
	mainwindow->emitWriteLog(str);
}

void ApplicationGlobal::writeLog(const QString &str)
{
	mainwindow->emitWriteLog(str);
}

IncrementalSearch *ApplicationGlobal::incremental_search()
{
	return &m->incremental_search;
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

struct AiCredentials {
	GenerativeAI::Credential operator () (GenerativeAI::Unknown const &provider) const
	{
		return {};
	}

	GenerativeAI::Credential operator () (GenerativeAI::OpenAI const &provider) const
	{
		GenerativeAI::Credential cred;
		if (global->appsettings.use_openai_api_key_environment_value) {
			cred.api_key = getenv(provider.envname().c_str());
		} else {
			cred.api_key = global->appsettings.openai_api_key.toStdString();
		}
		return cred;
	}

	GenerativeAI::Credential operator () (GenerativeAI::Anthropic const &provider) const
	{
		GenerativeAI::Credential cred;
		if (global->appsettings.use_anthropic_api_key_environment_value) {
			cred.api_key = getenv(provider.envname().c_str());
		} else {
			cred.api_key = global->appsettings.anthropic_api_key.toStdString();
		}
		return cred;
	}
	GenerativeAI::Credential operator () (GenerativeAI::Google const &provider) const
	{
		GenerativeAI::Credential cred;
		if (global->appsettings.use_google_api_key_environment_value) {
			cred.api_key = getenv(provider.envname().c_str());
		} else {
			cred.api_key = global->appsettings.google_api_key.toStdString();
		}
		return cred;
	}
	GenerativeAI::Credential operator () (GenerativeAI::DeepSeek const &provider) const
	{
		GenerativeAI::Credential cred;
		if (global->appsettings.use_deepseek_api_key_environment_value) {
			cred.api_key = getenv(provider.envname().c_str());
		} else {
			cred.api_key = global->appsettings.deepseek_api_key.toStdString();
		}
		return cred;
	}
	GenerativeAI::Credential operator () (GenerativeAI::OpenRouter const &provider) const
	{
		GenerativeAI::Credential cred;
		if (global->appsettings.use_openrouter_api_key_environment_value) {
			cred.api_key = getenv(provider.envname().c_str());
		} else {
			cred.api_key = global->appsettings.openrouter_api_key.toStdString();
		}
		return cred;
	}

	GenerativeAI::Credential operator () (GenerativeAI::Ollama const &provider) const
	{
		GenerativeAI::Credential cred;
		cred.api_key = "aonymous";
		return cred;
	}

	static GenerativeAI::Credential credential(GenerativeAI::Provider const &provider)
	{
		return std::visit(AiCredentials{}, provider);
	}

};

GenerativeAI::Credential ApplicationGlobal::get_ai_credential(GenerativeAI::Provider const &provider)
{
	return std::visit(AiCredentials{}, provider);
}

