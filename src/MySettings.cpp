#include "MySettings.h"
#include "ApplicationGlobal.h"

MySettings::MySettings(QObject *)
	: QSettings(global->config_file_path, QSettings::IniFormat)
{
}

