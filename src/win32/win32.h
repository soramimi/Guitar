#ifndef WIN32_H
#define WIN32_H
#include <QtGlobal>
#ifdef Q_OS_WIN

#include <QIcon>
#include <QString>
#include <string>

QString getModuleFileName();
QString getAppDataLocation();

int winRunCommand(QString const &cmd, QByteArray *out);
void setEnvironmentVariable(QString const &name, QString const &value);

QIcon winIconFromExtensionLarge(QString const &ext);
QIcon winIconFromExtensionSmall(QString const &ext);

QString getWin32HttpProxy();


struct Win32ShortcutData {
	std::wstring lnkpath;
	std::wstring targetpath;
	std::wstring arguments;
	std::wstring workingdir;
	std::wstring iconpath;
	int iconindex = 0;
	bool runas = false;
};

void createWin32Shortcut(Win32ShortcutData const &data);

#endif
#endif // WIN32_H
