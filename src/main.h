#ifndef MAIN_H
#define MAIN_H

#include <QColor>
#include <QString>

#define ORGANIZTION_NAME "soramimi.jp"
#define APPLICATION_NAME "Guitar"

extern QString application_data_dir;
extern QColor panel_bg_color;

struct ApplicationSettings {
	QString git_command;
	QString file_command;
	QString default_working_dir;
};

#endif // MAIN_H
