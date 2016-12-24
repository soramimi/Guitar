#include <QFileInfo>
#include <QtGlobal>
#ifdef Q_OS_WIN

#include "win32.h"
#include <Windows.h>
#include <ShlObj.h>

QString getModuleFileName()
{
	wchar_t tmp[300];
	DWORD n = GetModuleFileNameW(0, tmp, 300);
	return QString::fromUtf16((ushort const *)tmp, n);
}

QString getModuleFileDir()
{
	QString path = getModuleFileName();
	int i = path.lastIndexOf('\\');
	int j = path.lastIndexOf('/');
	if (i < j) i = j;
	if (i > 0) path = path.mid(0, i);
	return path;
}

QString getAppDataLocation()
{
	wchar_t tmp[300];
	if (SHGetSpecialFolderPathW(0, tmp, CSIDL_APPDATA, TRUE)) {
		return QString::fromUtf16((ushort const *)tmp);
	}
	return QString();
}

#endif
