#include "../src/FileType.h"
#include "../src/FileTypeInterface.h"
#include <QCoreApplication>
#include <QDebug>
#include <QPluginLoader>
#include <memory>


int main(int argc, char **argv)
{
	QCoreApplication a(argc, argv);

	QPluginLoader loader("filetypeplugind");
	FileTypeInterface *plugin = dynamic_cast<FileTypeInterface *>(loader.instance());
	if (plugin) {
		auto p = std::shared_ptr<FileType>(plugin->create());
		QString s = p->func("Hello,", " world");
		qDebug() << s;
	} else {
		qDebug() << "failed to load the plugin";
	}

	return 0;
}



