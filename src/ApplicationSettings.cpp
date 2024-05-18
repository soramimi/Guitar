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

} // namespace

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

QStringList ApplicationSettings::openai_gpt_models()
{
	QStringList list;
	list.append("gpt-3.5-turbo");
	list.append("gpt-4-turbo");
	list.append("gpt-4");
	list.sort();
	// list.append("gpt-4o");
	return list;
}

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
	s.endGroup();

	s.beginGroup("Network");
	GetValue<QString>(s, "ProxyType")                        >> as.proxy_type;
	GetValue<QString>(s, "ProxyServer")                      >> as.proxy_server;
	GetValue<bool>(s, "GetCommitterIcon")                    >> as.get_avatar_icon_from_network_enabled;
	GetValue<bool>(s, "AvatarProvider_gravatar")             >> as.avatar_provider.gravatar;
	GetValue<bool>(s, "AvatarProvider_libravatar")           >> as.avatar_provider.libravatar;
	s.endGroup();
	as.proxy_server = misc::makeProxyServerURL(as.proxy_server);

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

	s.beginGroup("Options");
	GetValue<bool>(s, "GenerateCommitMessageByAI")            >> as.generate_commit_message_by_ai;
	GetValue<QString>(s, "OpenAI_GPT_Model")                  >> as.openai_gpt_model;
	s.endGroup();
	
	if (as.openai_gpt_model.isEmpty()) {
		as.openai_gpt_model = "gpt-4";
	}
	
	as.openai_api_key = loadOpenAiApiKey();

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
	s.endGroup();

	s.beginGroup("Network");
	SetValue<QString>(s, "ProxyType")                        << this->proxy_type;
	SetValue<QString>(s, "ProxyServer")                      << misc::makeProxyServerURL(this->proxy_server);
	SetValue<bool>(s, "GetCommitterIcon")                    << this->get_avatar_icon_from_network_enabled;
	SetValue<bool>(s, "AvatarProvider_gravatar")             << this->avatar_provider.gravatar;
	SetValue<bool>(s, "AvatarProvider_libravatar")           << this->avatar_provider.libravatar;
	s.endGroup();

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
	SetValue<QString>(s, "OpenAI_GPT_Model")                  << this->openai_gpt_model;
	s.endGroup();

	if (0) { // ここでは保存しない
		saveOpenAiApiKey(this->openai_api_key);
	}
}
