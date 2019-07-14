#ifndef MAIN_H
#define MAIN_H

#include <QString>

#define ORGANIZATION_NAME "soramimi.jp"
#define APPLICATION_NAME "Guitar"

class ApplicationSettings {
public:
	QString git_command;
	QString file_command;
	QString gpg_command;
	QString default_working_dir;
	QString proxy_type;
	QString proxy_server;
	bool get_committer_icon = true;
	bool remember_and_restore_window_position = false;
	bool enable_high_dpi_scaling = true;
	bool automatically_fetch_when_opening_the_repository = true;
	unsigned int maximum_number_of_commit_item_acquisitions = 10000;
	static ApplicationSettings defaultSettings();
};

#endif // MAIN_H
