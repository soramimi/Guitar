#include "MySettings.h"
#include "ApplicationGlobal.h"

MySettings::MySettings()
	: QSettings(global->config_file_path, QSettings::IniFormat)
{
}

