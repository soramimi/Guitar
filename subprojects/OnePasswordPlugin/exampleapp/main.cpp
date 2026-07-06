#include "../src/OnePassword.h"
#include "../src/OnePasswordInterface.h"
#include <QCoreApplication>
#include <QDebug>
#include <QPluginLoader>
#include <memory>


int main(int argc, char **argv)
{
	putenv("OP_ACCOUNT_NAME=SHINICHI FUCHITA");
	
	QCoreApplication a(argc, argv);

#ifdef QT_DEBUG
	QPluginLoader loader("onepasswordplugind");
#else
	QPluginLoader loader("onepasswordplugin");
#endif
	OnePasswordInterface *plugin = dynamic_cast<OnePasswordInterface *>(loader.instance());
	if (plugin) {
		auto p = std::shared_ptr<OnePassword>(plugin->create());
		QString s = p->getapikey("SHINICHI FUCHITA", "op://API_KEY/Example/credential");
		qDebug() << s;
	} else {
		qDebug() << "failed to load the plugin";
	}

	return 0;
}



