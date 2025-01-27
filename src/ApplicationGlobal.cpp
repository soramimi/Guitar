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
