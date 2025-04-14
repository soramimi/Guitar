#include "ApplicationSettings.h"
#include "MySettings.h"
#include "common/joinpath.h"
#include "common/misc.h"
#include <QStandardPaths>

namespace {

template <typename T> class GetValue {
private:
public:
	MySettings &settings;
	QString name;
	GetValue(MySettings &s, QString const &name)
		: settings(s)
		, name(name)
	{
	}
};

template <typename T> void operator >> (GetValue<T> const &l, T &r)
{
	r = l.settings.value(l.name, r).template value<T>();
}

template <> void operator >> (GetValue<QColor> const &l, QColor &r)
{
	QString s = l.settings.value(l.name, QString()).template value<QString>(); // 文字列で取得
	if (s.startsWith('#')) {
		r = s;
	}
}

template <> void operator >> (GetValue<std::string> const &l, std::string &r)
{
	r = l.settings.value(l.name, QString::fromStdString(r)).toString().toStdString();
}

template <typename T> class SetValue {
private:
public:
	MySettings &settings;
	QString name;
	SetValue(MySettings &s, QString const &name)
		: settings(s)
		, name(name)
	{
	}
};

template <typename T> void operator << (SetValue<T> &&l, T const &r)
{
	l.settings.setValue(l.name, r);
}

template <> void operator << (SetValue<QColor> &&l, QColor const &r)
{
	QString s = QString::asprintf("#%02x%02x%02x", r.red(), r.green(), r.blue());
	l.settings.setValue(l.name, s);
}

template <> void operator << (SetValue<std::string> &&l, std::string const &r)
{
	l.settings.setValue(l.name, QString::fromStdString(r));
}

} // namespace

#if 0
QString ApplicationSettings::loadOpenAiApiKey()
{
	QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
	QSettings s(home / ".aicommits", QSettings::IniFormat);
	QString key = s.value("OPENAI_KEY").toString().trimmed();
	return key;
}

void ApplicationSettings::saveOpenAiApiKey(QString const &key)
{
	QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
	QSettings s(home / ".aicommits", QSettings::IniFormat);
	s.setValue("OPENAI_KEY", key.trimmed());
}
#endif

ApplicationSettings ApplicationSettings::loadSettings()
{
	ApplicationSettings as(defaultSettings());

	MySettings s;

	s.beginGroup("Global");
	GetValue<bool>(s, "SaveWindowPosition")                  >> as.remember_and_restore_window_position;
	GetValue<QString>(s, "DefaultWorkingDirectory")          >> as.default_working_dir;
	GetValue<QString>(s, "GitCommand")                       >> as.git_command;
	GetValue<QString>(s, "GpgCommand")                       >> as.gpg_command;
	GetValue<QString>(s, "SshCommand")                       >> as.ssh_command;
	GetValue<QString>(s, "TerminalCommand")                  >> as.terminal_command;
	GetValue<QString>(s, "ExplorerCommand")                  >> as.explorer_command;
	s.endGroup();

	s.beginGroup("UI");
	GetValue<bool>(s, "ShowLabels")                          >> as.show_labels;
	GetValue<bool>(s, "ShowGraph")                           >> as.show_graph;
	GetValue<bool>(s, "ShowAvatars")                         >> as.show_avatars;
	s.endGroup();

#if 0
	s.beginGroup("Network");
	GetValue<QString>(s, "ProxyType")                        >> as.proxy_type;
	GetValue<QString>(s, "ProxyServer")                      >> as.proxy_server;
	GetValue<bool>(s, "GetCommitterIcon")                    >> as.get_avatar_icon_from_network_enabled;
	GetValue<bool>(s, "AvatarProvider_gravatar")             >> as.avatar_provider.gravatar;
	GetValue<bool>(s, "AvatarProvider_libravatar")           >> as.avatar_provider.libravatar;
	s.endGroup();
	as.proxy_server = misc::makeProxyServerURL(as.proxy_server);
#endif

	s.beginGroup("Behavior");
	GetValue<bool>(s, "AutomaticFetch")                      >> as.automatically_fetch_when_opening_the_repository;
	GetValue<int>(s, "MaxCommitItemAcquisitions")            >> as.maximum_number_of_commit_item_acquisitions;
	s.endGroup();

	s.beginGroup("Visual");
	GetValue<QColor>(s, "LabelColorHead")                    >> as.branch_label_color.head;
	GetValue<QColor>(s, "LabelColorLocalBranch")             >> as.branch_label_color.local;
	GetValue<QColor>(s, "LabelColorRemoteBranch")            >> as.branch_label_color.remote;
	GetValue<QColor>(s, "LabelColorTag")                     >> as.branch_label_color.tag;
	s.endGroup();

	std::string ai_provider_name;
	std::string ai_model_name;

	s.beginGroup("Options");
	GetValue<bool>(s, "GenerateCommitMessageByAI")            >> as.generate_commit_message_by_ai;
	GetValue<bool>(s, "UseOpenAiApiKeyEnvironmentValue")      >> as.use_openai_api_key_environment_value;
	GetValue<bool>(s, "UseAnthropicApiKeyEnvironmentValue")   >> as.use_anthropic_api_key_environment_value;
	GetValue<bool>(s, "UseGoogleApiKeyEnvironmentValue")      >> as.use_google_api_key_environment_value;
	GetValue<bool>(s, "UseOpenRouterApiKeyEnvironmentValue")  >> as.use_openrouter_api_key_environment_value;
	GetValue<QString>(s, "OPENAI_API_KEY")                    >> as.openai_api_key;
	GetValue<QString>(s, "ANTHROPIC_API_KEY")                 >> as.anthropic_api_key;
	GetValue<QString>(s, "GOOGLE_API_KEY")                    >> as.google_api_key;
	GetValue<QString>(s, "DEEPSEEK_API_KEY")                  >> as.deepseek_api_key;
	GetValue<QString>(s, "OPENROUTER_API_KEY")                >> as.openrouter_api_key;
	GetValue<std::string>(s, "AiProvider")                    >> ai_provider_name;
	GetValue<std::string>(s, "AiModel")                       >> ai_model_name;
	GetValue<bool>(s, "IncrementalSearchWithMigemo")          >> as.incremental_search_with_miegemo;
	s.endGroup();

	auto providers = GenerativeAI::all_providers();
	auto it = std::find_if(providers.begin(), providers.end(), [&](auto const &p) { return GenerativeAI::provider_id(p) == ai_provider_name; });
	if (it != providers.end()) {
		as.ai_model = GenerativeAI::Model(*it, ai_model_name);
	} else {
		if (ai_provider_name.empty() && ai_model_name.empty()) {
			ai_model_name = "gpt-4o";
		}
		as.ai_model = GenerativeAI::Model::from_name(ai_model_name);
	}



#if 0
	as.openai_api_key = loadOpenAiApiKey();
#endif

	return as;
}

void ApplicationSettings::saveSettings() const
{
	MySettings s;

	s.beginGroup("Global");
	SetValue<bool>(s, "SaveWindowPosition")                  << this->remember_and_restore_window_position;
	SetValue<QString>(s, "DefaultWorkingDirectory")          << this->default_working_dir;
	SetValue<QString>(s, "GitCommand")                       << this->git_command;
	SetValue<QString>(s, "GpgCommand")                       << this->gpg_command;
	SetValue<QString>(s, "SshCommand")                       << this->ssh_command;
	SetValue<QString>(s, "TerminalCommand")                  << this->terminal_command;
	SetValue<QString>(s, "ExplorerCommand")                  << this->explorer_command;
	s.endGroup();

	s.beginGroup("UI");
	SetValue<bool>(s, "ShowLabels")                          << this->show_labels;
	SetValue<bool>(s, "ShowGraph")                           << this->show_graph;
	SetValue<bool>(s, "ShowAvatars")                         << this->show_avatars;
	s.endGroup();

#if 0
	s.beginGroup("Network");
	SetValue<QString>(s, "ProxyType")                        << this->proxy_type;
	SetValue<QString>(s, "ProxyServer")                      << misc::makeProxyServerURL(this->proxy_server);
	SetValue<bool>(s, "GetCommitterIcon")                    << this->get_avatar_icon_from_network_enabled;
	SetValue<bool>(s, "AvatarProvider_gravatar")             << this->avatar_provider.gravatar;
	SetValue<bool>(s, "AvatarProvider_libravatar")           << this->avatar_provider.libravatar;
	s.endGroup();
#endif

	s.beginGroup("Behavior");
	SetValue<bool>(s, "AutomaticFetch")                      << this->automatically_fetch_when_opening_the_repository;
	SetValue<int>(s, "MaxCommitItemAcquisitions")            << this->maximum_number_of_commit_item_acquisitions;
	s.endGroup();

	s.beginGroup("Visual");
	SetValue<QColor>(s, "LabelColorHead")                    << this->branch_label_color.head;
	SetValue<QColor>(s, "LabelColorLocalBranch")             << this->branch_label_color.local;
	SetValue<QColor>(s, "LabelColorRemoteBranch")            << this->branch_label_color.remote;
	SetValue<QColor>(s, "LabelColorTag")                     << this->branch_label_color.tag;
	s.endGroup();

	s.beginGroup("Options");
	SetValue<bool>(s, "GenerateCommitMessageByAI")            << this->generate_commit_message_by_ai;
	SetValue<bool>(s, "UseOpenAiApiKeyEnvironmentValue")      << this->use_openai_api_key_environment_value;
	SetValue<bool>(s, "UseAnthropicApiKeyEnvironmentValue")   << this->use_anthropic_api_key_environment_value;
	SetValue<bool>(s, "UseGoogleApiKeyEnvironmentValue")      << this->use_google_api_key_environment_value;
	SetValue<bool>(s, "UseDeepSeekApiKeyEnvironmentValue")    << this->use_deepseek_api_key_environment_value;
	SetValue<bool>(s, "UseOpenRouterApiKeyEnvironmentValue")  << this->use_openrouter_api_key_environment_value;
	SetValue<QString>(s, "OPENAI_API_KEY")                    << this->openai_api_key;
	SetValue<QString>(s, "ANTHROPIC_API_KEY")                 << this->anthropic_api_key;
	SetValue<QString>(s, "GOOGLE_API_KEY")                    << this->google_api_key;
	SetValue<QString>(s, "DEEPSEEK_API_KEY")                  << this->deepseek_api_key;
	SetValue<QString>(s, "OPENROUTER_API_KEY")                << this->openrouter_api_key;
	SetValue<std::string>(s, "AiProvider")                    << GenerativeAI::provider_id(this->ai_model.provider);
	SetValue<std::string>(s, "AiModel")                       << this->ai_model.long_name();
	SetValue<bool>(s, "IncrementalSearchWithMigemo")          << this->incremental_search_with_miegemo;
	s.endGroup();

	if (0) { // ここでは保存しない
#if 0
		saveOpenAiApiKey(this->openai_api_key);
#endif
	}
}
