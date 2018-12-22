#ifndef WIN32_H
#define WIN32_H
#include <QtGlobal>
#ifdef Q_OS_WIN

#include <QIcon>
#include <QString>

QString getModuleFileName();
QString getAppDataLocation();

int winRunCommand(QString const &cmd, QByteArray *out);
void setEnvironmentVariable(QString const &name, QString const &value);

QIcon winIconFromExtensionLarge(QString const &ext);
QIcon winIconFromExtensionSmall(QString const &ext);

QString getWin32HttpProxy();

#endif
#endif // WIN32_H
