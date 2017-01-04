#ifndef MAIN_H
#define MAIN_H

#include <QString>

#define ORGANIZTION_NAME "soramimi.jp"
#define APPLICATION_NAME "Guitar"

extern QString application_data_dir;

struct ApplicationSettings {
	QString git_command;
	QString file_command;
	QString default_working_dir;

};

#endif // MAIN_H
