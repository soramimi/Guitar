#ifndef LOADPLUGIN_H
#define LOADPLUGIN_H

#include <Logger.h>
#include <QCoreApplication>
#include <QDebug>
#include <QPluginLoader>
#include <QString>
#include <joinpath.h>
#include <memory>
#include <exception>

template <typename ImplType, typename Interface> std::shared_ptr<ImplType> loadPlugin(std::string name, QString *error = nullptr)
{
	std::shared_ptr<ImplType> ret;

#ifdef QT_DEBUG
	name.push_back('d');
#endif

#ifdef Q_OS_WIN
	QString path = QString::fromStdString(name);
#else
	QString path = QCoreApplication::applicationDirPath() / QString::fromStdString("../lib/soramimi.jp/lib" + name + ".so");
#endif
	
	char const *const c_name = name.c_str();
	
	logprintf(LOG_DEFAULT, "loading the plugin %s", path.toStdString().c_str());
	auto fail = [&](QString const &reason) -> std::shared_ptr<ImplType> {
		QString message = QString::fromStdString(name) + " (" + path + "): " + reason;
		if (error) *error = message;
		logprint(LOG_BOTH, message.toStdString());
		return {};
	};
	if (error) error->clear();
	try {
		QPluginLoader loader(path);
		QObject *instance = loader.instance();
		if (!instance) return fail(loader.errorString());
		Interface *plugin = dynamic_cast<Interface *>(instance);
		if (!plugin) return fail("Incompatible plugin interface.");
		ret.reset(plugin->create());
		if (!ret) return fail("Plugin factory returned null.");
		if (!ret->open()) return fail("Plugin initialization failed.");
		qDebug() << QString::asprintf("%s is loaded successfully.", c_name);
	} catch (std::exception const &e) {
		return fail(QString::fromUtf8(e.what()));
	} catch (QString const &e) {
		return fail(e);
	} catch (...) {
		return fail("Unknown plugin initialization error.");
	}

	return ret;
}


#endif // LOADPLUGIN_H
