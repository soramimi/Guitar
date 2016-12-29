#ifndef WIN32_H
#define WIN32_H
#include <QtGlobal>

#include <QString>

QString getModuleFileName();
QString getModuleFileDir();
QString getAppDataLocation();

int winRunCommand(QString const &cmd, QByteArray *out);

#endif // WIN32_H
