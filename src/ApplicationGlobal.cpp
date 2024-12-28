#include "ApplicationGlobal.h"
#include "MainWindow.h"
#include <memory>
#include <QFileIconProvider>


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
}

void ApplicationGlobal::writeLog(const std::string_view &str, bool record)
{
	mainwindow->writeLog(str, record);
}

void ApplicationGlobal::writeLog(const QString &str, bool record)
{
	mainwindow->writeLog(str, record);
}

bool ApplicationGlobal::isMainThread()
{
	return QThread::currentThread() == QCoreApplication::instance()->thread();
}
