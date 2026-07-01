#ifndef LOADPLUGIN_H
#define LOADPLUGIN_H

#include <Logger.h>
#include <QCoreApplication>
#include <QPluginLoader>
#include <QString>
#include <joinpath.h>
#include <memory>

template <typename ImplType, typename Interface> std::shared_ptr<ImplType> loadPlugin(std::string name)
{
	std::shared_ptr<ImplType> ret;

#ifdef QT_DEBUG
	name.push_back('d');
#endif

#ifdef Q_OS_WIN
	QString path = QString::fromStdString(name);
#else
	QString path = QCoreApplication::applicationDirPath() / QString::fromStdString("../lib/lib" + name + ".so");
#endif
	
	char const *const c_name = name.c_str();
	
	logprintf(LOG_DEFAULT, "loading the plugin %s", path.toStdString().c_str());
	QPluginLoader loader(path);
	Interface *plugin = dynamic_cast<Interface *>(loader.instance());
	if (plugin) {
		ret = std::shared_ptr<ImplType>(plugin->create());
		if (ret->open()) {
			qDebug() << QString::asprintf("%s is loaded successfully.", c_name);
		} else {
			qDebug() << QString::asprintf("Failed to load the plugin %s", c_name);
			return {};
		}
	} else {
		qDebug() << loader.errorString();
		return {};
	}
	
	return ret;
}


#endif // LOADPLUGIN_H
