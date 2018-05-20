#ifndef MAIN_H
#define MAIN_H

#include <QString>

#define ORGANIZTION_NAME "soramimi.jp"
#define APPLICATION_NAME "Guitar"

struct ApplicationSettings {
	QString git_command;
	QString file_command;
	QString gpg_command;
	QString default_working_dir;
	QString proxy_type;
	QString proxy_server;
	QString theme;
	bool get_committer_icon = false;
	bool remember_and_restore_window_position = false;
	bool enable_high_dpi_scaling = true;
	bool automatically_fetch_when_opening_the_repository = true;
	int maximum_number_of_commit_item_acquisitions = 10000;
	static ApplicationSettings defaultSettings();
};

#endif // MAIN_H
