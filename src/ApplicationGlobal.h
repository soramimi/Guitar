#ifndef APPLICATIONGLOBAL_H
#define APPLICATIONGLOBAL_H

#include <QColor>
#include <QString>
#include "Theme.h"

struct ApplicationGlobal {
	bool start_with_shift_key = false;
	QString organization_name;
	QString application_name;
	QString generic_config_dir;
	QString app_config_dir;
	QString config_file_path;
	QString file_command;
	QString gpg_command;
	QColor panel_bg_color;
	ThemePtr theme;
};

extern ApplicationGlobal *global;

#endif // APPLICATIONGLOBAL_H
