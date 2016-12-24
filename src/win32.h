#ifndef WIN32_H
#define WIN32_H
#include <QtGlobal>
#ifdef Q_OS_WIN

#include <QString>

QString getModuleFileName();
QString getModuleFileDir();
QString getAppDataLocation();

#endif
#endif // WIN32_H
