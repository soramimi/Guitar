#include "MySettings.h"
#include "main.h"
#include "common/joinpath.h"
#include "ApplicationGlobal.h"
#include <QApplication>
#include <QDir>
#include <QString>
#include <QStandardPaths>
#include <QDebug>

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
	: QSettings(joinpath(makeApplicationDataDir(), global->application_name + ".ini"), QSettings::IniFormat)
{
}

