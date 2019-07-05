#include "MySettings.h"
#include <common/joinpath.h>
#include <QApplication>
#include <QDir>
#include <QString>
#include <QStandardPaths>

QString makeApplicationDataDir()
{
	QString dir;
	dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
	if (!QFileInfo(dir).isDir()) {
		QDir().mkpath(dir);
	}
	return dir;
}

MySettings::MySettings(QObject *)
	: QSettings(joinpath(makeApplicationDataDir(), qApp->applicationName() + ".ini"), QSettings::IniFormat)
{
}

