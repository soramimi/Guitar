#include "../src/IncrementalSearch.h"
#include "../src/IncrementalSearchInterface.h"
#include <QCoreApplication>
#include <QDebug>
#include <QPluginLoader>
#include <memory>

#include <dlfcn.h>

int main(int argc, char **argv)
{
	QCoreApplication a(argc, argv);
	
	// void *lib = dlopen("./libincrementalsearchplugin.so", RTLD_NOW | RTLD_GLOBAL);
	// int (*fn_libmain)();
	// (void *&)fn_libmain = dlsym(lib, "libmain");
	
	QPluginLoader loader("incrementalsearchplugin");
	IncrementalSearchInterface *plugin = dynamic_cast<IncrementalSearchInterface *>(loader.instance());
	if (plugin) {
		auto p = std::shared_ptr<IncrementalSearch>(plugin->create());
		if (p->open()) {
			std::string s = p->convertRomajiToKatakana("wagahaihanekodearu");
			QString q = QString::fromStdString(s);
			qDebug() << s;
		}
	} else {
		qDebug() << loader.errorString();
	}

	return 0;
}



