#ifndef APPLICATIONSETTINGS_H
#define APPLICATIONSETTINGS_H

#include <QColor>
#include <QString>
#include <map>

#define ORGANIZATION_NAME "soramimi.jp"
#define APPLICATION_NAME "Guitar"

class MySettings;

namespace GenerativeAI {
enum class ProviderID;
struct Model;
struct Credential;
class ModelURI;
}

class ApplicationBasicData {
public:
	QString organization_name = ORGANIZATION_NAME;
	QString application_name = APPLICATION_NAME;
	QString application_file_path;
	QString generic_config_dir;
	QString app_config_dir;
	QString log_dir;
	QString config_file_path;
};

class AiApiKeys {
public:
	enum class KeyFrom {
		Environment, // 環境変数から取得
		LocalSecret, // ユーザーが設定画面で入力
		Default = Environment
	};
	static QString symbolKeyFrom(KeyFrom keyfrom);
	static KeyFrom parseKeyFrom(QString const &symbol);

	struct Item {
		KeyFrom from = KeyFrom::Default;
		std::string api_key;
	};

	std::map<std::string, AiApiKeys::Item> map; // key is env_name

	bool load(MySettings *s);
	bool save(MySettings *s) const;
};

class ApplicationSettings {
public:
	bool enable_trace_log = false;
	bool use_custom_log_dir = false;
	QString custom_log_dir;

	QString git_command;
	QString gpg_command;
	QString ssh_command;
	QString terminal_command;
	QString explorer_command;
	QString default_working_dir;
	QStringList favorite_working_dirs;
	QString proxy_type;
	QString proxy_server;

	bool generate_commit_message_with_ai = false;
	AiApiKeys ai_api_keys;
	std::shared_ptr<GenerativeAI::Model> ai_model;
	std::tuple<std::vector<GenerativeAI::Model const *>, int> ai_models() const;

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
	bool show_avatars = true;

	bool incremental_search_with_miegemo = false;

	struct {
		QColor head;
		QColor local;
		QColor remote;
		QColor tag;
	} branch_label_color;

	struct {
		QColor filtered_bg;
		QColor highlight_bg;
	} incremental_search_color;

#ifdef Q_OS_WIN
	enum class ConsoleBackend {
		Undefined,
		ConPty,
		ConPtyWithWorker,
		WinPty,
	};
	ConsoleBackend console_backend = ConsoleBackend::Undefined;
#endif

	ApplicationSettings();

	static ApplicationSettings loadSettings();
	void saveSettings() const;

	static ApplicationSettings defaultSettings();

#if 0
	static QString loadOpenAiApiKey();
	static void saveOpenAiApiKey(const QString &key);
#endif
};

#endif // APPLICATIONSETTINGS_H
