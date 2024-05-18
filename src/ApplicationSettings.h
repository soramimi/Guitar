#ifndef APPLICATIONSETTINGS_H
#define APPLICATIONSETTINGS_H

#include <QColor>
#include <QString>

#define ORGANIZATION_NAME "soramimi.jp"
#define APPLICATION_NAME "Guitar"

class ApplicationBasicData {
public:
	QString organization_name = ORGANIZATION_NAME;
	QString application_name = APPLICATION_NAME;
	QString this_executive_program;
	QString generic_config_dir;
	QString app_config_dir;
	QString config_file_path;
};

class ApplicationSettings {
public:
	QString git_command;
	QString gpg_command;
	QString ssh_command;
	QString terminal_command;
	QString explorer_command;
	QString default_working_dir;
	QStringList favorite_working_dirs;
	QString proxy_type;
	QString proxy_server;

	bool generate_commit_message_by_ai = false;
	QString openai_api_key;
	QString openai_gpt_model;

	bool get_avatar_icon_from_network_enabled = true;
	struct {
		bool gravatar = true; // www.gravatar.com
		bool libravatar = true; // www.libravatar.org
	} avatar_provider;

	bool remember_and_restore_window_position = false;
	bool automatically_fetch_when_opening_the_repository = true;
	int maximum_number_of_commit_item_acquisitions = 10000;
	bool show_labels = true;
	bool show_graph = true;

	struct {
		QColor head;
		QColor local;
		QColor remote;
		QColor tag;
	} branch_label_color;

	static ApplicationSettings loadSettings();
	void saveSettings() const;

	static ApplicationSettings defaultSettings();

	static QString loadOpenAiApiKey();
	static void saveOpenAiApiKey(const QString &key);
	
	static QStringList openai_gpt_models();
};

#endif // APPLICATIONSETTINGS_H
