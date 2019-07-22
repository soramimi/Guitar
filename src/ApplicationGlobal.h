#ifndef APPLICATIONGLOBAL_H
#define APPLICATIONGLOBAL_H

#include <QColor>
#include <QString>
#include "Theme.h"

class MainWindow;

struct ApplicationGlobal {
	MainWindow *mainwindow = nullptr;
	bool start_with_shift_key = false;
	QString organization_name;
	QString application_name;
	QString language_id;
	QString theme_id;
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
