#include "../src/IncrementalSearch.h"
#include "../src/IncrementalSearchInterface.h"
#include <QCoreApplication>
#include <QDebug>
#include <QPluginLoader>
#include <memory>

#include <dlfcn.h>

int main(int argc, char **argv)
{
	putenv(const_cast<char *>("QT_ASSUME_STDERR_HAS_CONSOLE=1"));
	
	QCoreApplication a(argc, argv);
	
	// void *lib = dlopen("./libincrementalsearchplugin.so", RTLD_NOW | RTLD_GLOBAL);
	// int (*fn_libmain)();
	// (void *&)fn_libmain = dlsym(lib, "libmain");
	
	QString name = "incrementalsearchplugin";
#ifdef QT_DEBUG
	name += 'd';
#endif
	QPluginLoader loader(name);
	IncrementalSearchInterface *plugin = dynamic_cast<IncrementalSearchInterface *>(loader.instance());
	if (plugin) {
		auto p = std::shared_ptr<IncrementalSearch>(plugin->create());
		if (p->open()) {
			{
				std::string s = p->convertRomajiToKatakana("wagahaihanekodearu");
				puts(s.c_str());
			}
			{
				IncrementalSearchFilter filter = p->makeFilter("neko");
				incrementalsearch::Result r = p->match("吾輩は猫である", filter);
				assert(r);
				assert(r.parts.size() == 3);
				assert(r.parts[0].text == "吾輩は" && !r.parts[0].match);
				assert(r.parts[1].text == "猫" && r.parts[1].match);
				assert(r.parts[2].text == "である" && !r.parts[2].match);
			}
		}
	} else {
		qDebug() << loader.errorString();
	}

	return 0;
}



