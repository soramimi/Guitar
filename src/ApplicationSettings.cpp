#include "ApplicationSettings.h"
#include "ApplicationGlobal.h"
#include "Logger.h"
#include "MySettings.h"
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <ai/GenerativeAI.h>
#include <common/fmt.h>
#include <common/joinpath.h>
#include <common/misc.h>
#include <common/q/helper.h>
#include <vector>

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

std::tuple<std::vector<GenerativeAI::Model const *>, int> ApplicationSettings::ai_models() const
{
	std::vector<GenerativeAI::Model> const &list = GenerativeAI::ai_model_presets();
	std::vector<GenerativeAI::Model const *> newlist;
	for (GenerativeAI::Model const &item : list) {
		newlist.push_back(&item);
	}
	int index;
	for (index = 0; index < (int)list.size(); index++) {
		if (list[index].model_uri() == ai_model->model_uri()) {
			return {newlist, index};
		}
	}
	newlist.push_back(ai_model.get());
	return {newlist, index};
}

ApplicationSettings::ApplicationSettings()
{
	ai_model = std::make_shared<GenerativeAI::Model>();
}

#if 0
static inline QString UPPER(QString const &s)
{
	return s.toUpper();
}
#endif

ApplicationSettings ApplicationSettings::loadSettings()
{
	ApplicationSettings as(defaultSettings());

	MySettings s;

	// load api keys

	if (!as.ai_api_keys.load(&s)) {
		logprintf(LOG_DEFAULT, "Failed to load AI API keys\n");
	}

	//

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
	std::string ai_model_uri;

	s.beginGroup("AI");
	GetValue<bool>(s, "GenerateCommitMessageWithAI")         >> as.generate_commit_message_with_ai;
	GetValue<std::string>(s, "Provider")                     >> ai_provider_name;
	GetValue<std::string>(s, "ModelURI")                     >> ai_model_uri;
	s.endGroup();

#ifdef Q_OS_WIN
	QString console_backend;
	s.beginGroup("Windows");
	GetValue<QString>(s, "ConsoleBackend")               >> console_backend;
	s.endGroup();
	if (console_backend == "WinPty") {
		as.console_backend = ConsoleBackend::WinPty;
	} else if (console_backend == "ConPtyWithWorkerProcess") {
		as.console_backend = ConsoleBackend::ConPtyWithWorker;
	} else {
		as.console_backend = ConsoleBackend::ConPty;
	}
#endif

	// 選択されたモデルを取得

	auto Info = [&](std::string const &name)-> GenerativeAI::ProviderInfo const * {
		std::vector<GenerativeAI::ProviderInfo> const &infos = GenerativeAI::complete_provider_table();
		for (auto const &info : infos) {
			if (info.tag == name) {
				return &info;
			}
		}
		return nullptr;
	};
	GenerativeAI::ProviderInfo const *info = Info(ai_provider_name);

	if (info) {
		*as.ai_model = GenerativeAI::Model(info->id, ai_model_uri);
	} else {
		if (ai_provider_name.empty() && ai_model_uri.empty()) {
			ai_model_uri = GenerativeAI::Model::default_model();
		}
		*as.ai_model = GenerativeAI::Model::from_name(ai_model_uri);
	}

	return as;
}

void ApplicationSettings::saveSettings() const
{
	MySettings s;

	// save api keys

	if (!ai_api_keys.save(&s)) {
		logprintf(LOG_DEFAULT, "Failed to save AI API keys\n");
	}

	//

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

	s.beginGroup("AI");
	SetValue<bool>(s, "GenerateCommitMessageWithAI")         << this->generate_commit_message_with_ai;
	SetValue<std::string>(s, "Provider")                     << this->ai_model->provider_info_->tag;
	SetValue<std::string>(s, "ModelURI")                     << this->ai_model->model_uri().string;
	s.endGroup();

#ifdef Q_OS_WIN
	QString cb;
	if (this->console_backend == ConsoleBackend::WinPty) {
		cb = "WinPty";
	} else if (this->console_backend == ConsoleBackend::ConPtyWithWorker) {
		cb = "ConPtyWithWorkerProcess";
	} else {
		cb = "ConPty";
	}
	s.beginGroup("Windows");
	SetValue<QString>(s, "ConsoleBackend")               << cb;
	s.endGroup();
#endif
}

QString AiApiKeys::symbolKeyFrom(KeyFrom keyfrom)
{
	switch (keyfrom) {
	case KeyFrom::Environment: return "environment";
	case KeyFrom::LocalSecret: return "localsecret";
	}
	return {};
}

AiApiKeys::KeyFrom AiApiKeys::parseKeyFrom(QString const &symbol)
{
	if (symbol == "environment") {
		return KeyFrom::Environment;
	} else if (symbol == "localsecret") {
		return KeyFrom::LocalSecret;
	}
	return KeyFrom::Default;
}

bool AiApiKeys::load(MySettings *s)
{
	map.clear();

	QString secret_dir = global->app_config_dir / secret_sub_dir;
	if (QFileInfo(secret_dir).isDir()) {
		QString ini_file = secret_dir / api_keys_ini;
		QFile file(ini_file);
		if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
			while (!file.atEnd()) {
				QByteArray line = file.readLine().trimmed();
				int eq = line.indexOf('=');
				if (eq > 0) {
					std::string envname = line.left(eq).trimmed().toStdString();
					std::string api_key = line.mid(eq + 1).trimmed().toStdString();
					map[envname].api_key = api_key;
				}
			}

			{
				s->beginGroup("AI");
				for (auto &pair : map) {
					std::string const &env_name = pair.first;
					AiApiKeys::Item *aikey = &pair.second;
					auto keyfrom = s->value((QS)fmt("Use_%s")(env_name)).toString();
					aikey->from = parseKeyFrom(keyfrom);
				}
				s->endGroup();
			}

			return true;
		}
	}
	return false;
}


bool AiApiKeys::save(MySettings *s) const
{
	auto MKPATH = [&](const QString &path) {
		if (!QFileInfo(path).isDir()) {
			if (!QDir().mkpath(path)) {
				qDebug() << "Failed to create directory:" << path;
			}
		}
		return QFileInfo(path).isDir();
	};

	bool ret = false;

	QString secret_dir = global->app_config_dir / secret_sub_dir;
	if (MKPATH(secret_dir)) {
		QString ini_file = secret_dir / api_keys_ini;
		QFile file(ini_file);
		if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
			for (auto const &pair : map) {
				std::string line = fmt("%s=%s\n")(pair.first)(misc::trimmed(pair.second.api_key));
				file.write(line.c_str(), line.size());
			}
			file.close();
			ret = true;
		}
		QFile(secret_dir).setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner); // 所有者のみ読み書きと実行可
		QFile(ini_file).setPermissions(QFile::ReadOwner | QFile::WriteOwner); // 所有者のみ読み書き可

		{
			s->beginGroup("AI");
			for (auto const &pair : map) {
				std::string const &envname = pair.first;
				AiApiKeys::Item const &aikey = pair.second;
				QString keyfrom = symbolKeyFrom(aikey.from);
				s->setValue((QS)fmt("Use_%s")(envname), keyfrom);
			}
			s->endGroup();
		}
	}

	return ret;
}

