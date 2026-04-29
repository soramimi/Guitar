#include "ApplicationSettings.h"
#include "ApplicationGlobal.h"
#include "MySettings.h"
#include "common/joinpath.h"
#include "common/misc.h"
#include "common/strformat.h"
#include "common/q/helper.h"
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace {

constexpr static char const secret_sub_dir[] = ".secret";
constexpr static char const api_keys_ini[] = "apikeys.ini";

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
	r = l.settings.value(l.name, QString::fromStdString(r)).toString().trimmed().toStdString();
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
	l.settings.setValue(l.name, QString::fromStdString(r).trimmed());
}

} // namespace

std::tuple<std::vector<GenerativeAI::Model>, int> ApplicationSettings::ai_models() const
{
	std::vector<GenerativeAI::Model> list = GenerativeAI::ai_model_presets();
	int index;
	for (index = 0; index < (int)list.size(); index++) {
		if (list[index].long_name() == ai_model.long_name()) {
			return {list, index};
		}
	}
	list.push_back(ai_model);
	return {list, index};
}

static inline QString UPPER(QString const &s)
{
	return s.toUpper();
}

ApplicationSettings ApplicationSettings::loadSettings()
{
	ApplicationSettings as(defaultSettings());

	MySettings s;

	s.beginGroup("Global");
	GetValue<bool>(s, "EnableTraceLog")                      >> as.enable_trace_log;
	GetValue<bool>(s, "UseCustomLogDir")                     >> as.use_custom_log_dir;
	GetValue<QString>(s, "CustomLogDir")                     >> as.custom_log_dir;

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
	{
		std::vector<GenerativeAI::ProviderInfo> const &table = GenerativeAI::provider_table();
		for (GenerativeAI::ProviderInfo const &info : table) {
			if (info.env_name.empty()) {
				Q_ASSERT(info.symbol.empty()); // env_nameが空のときはsymbolも空であるべき
				continue; // 環境変数名が空でないプロバイダーのみ対象とする
			}
			// Q_ASSERT(!info.symbol.empty()); // env_nameが空でないときはsymbolも空であってはならない
			// ↑空であってもよい
			as.ai_api_keys[info.aiid] = {};
			ApplicationSettings::AiApiKey *aikey = &as.ai_api_keys[info.aiid];
			bool from = false;
			GetValue<bool>(s, (QS)fmt("Use%sApiKeyEnvironmentValue")(info.symbol)) >> from;
			aikey->from = ApplicationSettings::ApiKeyFrom::EnvValue;
			if (from) {
				aikey->from = ApplicationSettings::ApiKeyFrom::UserInput;
			}
			// GetValue<std::string>(s, UPPER((QS)info.env_name)) >> aikey->api_key;
		}
	}
	GetValue<std::string>(s, "AiProvider")                    >> ai_provider_name;
	GetValue<std::string>(s, "AiModel")                       >> ai_model_name;
	GetValue<bool>(s, "IncrementalSearchWithMigemo")          >> as.incremental_search_with_miegemo;
	s.endGroup();

	// load api keys

	QString secret_dir = global->app_config_dir / secret_sub_dir;
	if (QFileInfo(secret_dir).isDir()) {
		std::map<std::string, std::string> keys;

		QString ini_file = secret_dir / api_keys_ini;
		QFile file(ini_file);
		if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
			while (!file.atEnd()) {
				QByteArray line = file.readLine().trimmed();
				int eq = line.indexOf('=');
				if (eq > 0) {
					std::string name = line.left(eq).toStdString();
					std::string value = line.mid(eq + 1).toStdString();
					keys[name] = value;
				}
			}
		}

		std::vector<GenerativeAI::ProviderInfo> const &table = GenerativeAI::provider_table();
		for (GenerativeAI::ProviderInfo const &info : table) {
			if (info.env_name.empty()) {
				continue;
			}
			auto it = keys.find(UPPER((QS)info.env_name).toStdString());
			if (it != keys.end()) {
				as.ai_api_keys[info.aiid].api_key = misc::trimmed(it->second);
			}
		}
	}

	//

	auto Info = [&](std::string const &name)-> GenerativeAI::ProviderInfo const * {
		std::vector<GenerativeAI::ProviderInfo> const &infos = GenerativeAI::provider_table();
		for (auto const &info : infos) {
			if (info.tag == name) {
				return &info;
			}
		}
		return nullptr;
	};
	GenerativeAI::ProviderInfo const *info = Info(ai_provider_name);

	if (info) {
		as.ai_model = GenerativeAI::Model(info->aiid, ai_model_name);
	} else {
		if (ai_provider_name.empty() && ai_model_name.empty()) {
			ai_model_name = GenerativeAI::Model::default_model();
		}
		as.ai_model = GenerativeAI::Model::from_name(ai_model_name);
	}

	return as;
}

void ApplicationSettings::saveSettings() const
{
	MySettings s;

	s.beginGroup("Global");
	SetValue<bool>(s, "EnableTraceLog")                      << this->enable_trace_log;
	SetValue<bool>(s, "UseCustomLogDir")                     << this->use_custom_log_dir;
	SetValue<QString>(s, "CustomLogDir")                     << this->custom_log_dir;

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

	{
		std::vector<GenerativeAI::ProviderInfo> const &table = GenerativeAI::provider_table();
		for (GenerativeAI::ProviderInfo const &info : table) {
			if (info.env_name.empty()) {
				Q_ASSERT(info.symbol.empty()); // env_nameが空のときはsymbolも空であるべき
				continue; // 環境変数名が空でないプロバイダーのみ対象とする
			}
			// Q_ASSERT(!info.symbol.empty()); // env_nameが空でないときはsymbolも空であってはならない
			// ↑空であってもよい
			auto it = this->ai_api_keys.find(info.aiid);
			if (it == this->ai_api_keys.end()) continue;
			ApplicationSettings::AiApiKey const *aikey = &it->second;
			bool from = (aikey->from == ApplicationSettings::ApiKeyFrom::UserInput);
			SetValue<bool>(s, (QS)fmt("Use%sApiKeyEnvironmentValue")(info.symbol)) << from;
			SetValue<std::string>(s, UPPER((QS)info.env_name)) << aikey->api_key;
		}

	}
	SetValue<std::string>(s, "AiProvider")                    << this->ai_model.provider_info_->tag;
	SetValue<std::string>(s, "AiModel")                       << this->ai_model.long_name();
	SetValue<bool>(s, "IncrementalSearchWithMigemo")          << this->incremental_search_with_miegemo;
	s.endGroup();

	auto MKPATH = [&](const QString &path) {
		if (!QFileInfo(path).isDir()) {
			if (!QDir().mkpath(path)) {
				qDebug() << "Failed to create directory:" << path;
			}
		}
		return QFileInfo(path).isDir();
	};

	// save api keys

	QString secret_dir = global->app_config_dir / secret_sub_dir;
	if (MKPATH(secret_dir)) {
		std::map<std::string, std::string> keys;
		{
			for (auto const &pair : this->ai_api_keys) {
				GenerativeAI::AI aiid = pair.first;
				ApplicationSettings::AiApiKey const &aikey = pair.second;
				GenerativeAI::ProviderInfo const *info = GenerativeAI::provider_info(aiid);
				if (!info) continue;
				if (info->symbol.empty()) continue;
				keys[info->env_name] = aikey.api_key;
			}
		}
		QString ini_file = secret_dir / api_keys_ini;
		QFile file(ini_file);
		if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
			// std::map<GenerativeAI::AI, AiApiKey> ai_api_keys;
			for (auto const &pair : keys) {
				std::string line = fmt("%s=%s\n")(pair.first)(pair.second);
				file.write(line.c_str(), line.size());
			}
			file.close();
		}
		QFile(secret_dir).setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner); // 所有者のみ読み書きと実行可
		QFile(ini_file).setPermissions(QFile::ReadOwner | QFile::WriteOwner); // 所有者のみ読み書き可
	}
}
