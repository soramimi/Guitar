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
#include <easycrypto/easycrypto.h>
#include <common/q/helper.h>
#include <QBuffer>
#include <vector>
#include "MemoryReader.h"

namespace {

constexpr static char const secret_sub_dir[] = ".secret";
constexpr static char const api_keys_bin[] = "apikeys.bin";

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

ApplicationSettings::ApplicationSettings()
{
	ai_model = std::make_shared<GenerativeAI::Model>();
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

	s.beginGroup("AI");
	GetValue<bool>(s, "GenerateCommitMessageWithAI")         >> as.generate_commit_message_with_ai;
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
	
	return as;
}

void ApplicationSettings::saveSettings() const
{
	MySettings s;

	// save api keys

#if 0
	if (ai_api_keys_changed) {
		if (!ai_api_keys.save(std::string(api_key_obfuscation_key), &s)) {
			logprintf(LOG_DEFAULT, "Failed to save AI API keys\n");
		}
	}
#endif

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
	if (1) { // remove deplecated settings
		QStringList keys = s.allKeys();
		for (QString const &key : keys) {
			if (key == "DefaultModelGUID") continue; // don't remove
			s.remove(key);
		}
	}
	SetValue<bool>(s, "GenerateCommitMessageWithAI")         << this->generate_commit_message_with_ai;
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

static constexpr char const *api_keys_bin_filename = "apikeys.bin";

bool AiApiKeys::load(localvault::Vault *vault)
{
	QString dir = global->app_secret_config_dir;
	
	QFile file(dir / api_keys_bin_filename);
	if (file.open(QIODevice::ReadOnly)) {
		localvault::Blob encrypted;
		QByteArray ba = file.readAll();
		encrypted.assign(ba.constData(), ba.constData() + ba.size());
		file.close();

		localvault::SecureBuffer decrypted;
		localvault::VaultError err = vault->decryptToSecureBuffer(encrypted, &decrypted);
		if (err == localvault::VaultError::None) {
			MemoryReader buffer((char const *)decrypted.data(), decrypted.size());
			buffer.open(QIODevice::ReadOnly);
			while (!buffer.atEnd()) {
				QByteArray line = buffer.readLine().trimmed();
				int eq = line.indexOf('=');
				if (eq > 0) {
					std::string envname = line.left(eq).trimmed().toStdString();
					std::string api_key = line.mid(eq + 1).trimmed().toStdString();
					map[envname].api_key = api_key;
				}
			}
			return true;
		}
	}
	return false;
}

bool AiApiKeys::save(localvault::Vault *vault)
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
	
	QByteArray ba;
	{
		QBuffer buffer;
		buffer.open(QIODevice::WriteOnly);
		for (auto const &pair : map) {
			std::string line = fmt("%s=%s\n")(pair.first)(misc::trimmed(pair.second.api_key));
			buffer.write(line.c_str(), line.size());
		}
		ba = buffer.buffer();
	}
	
	QString secret_dir = global->app_secret_config_dir;
	
	localvault::SecureBuffer secret((void const *)ba.constData(), ba.size());
	QFile file(secret_dir / api_keys_bin_filename);
	if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		localvault::Blob encrypted;
		localvault::VaultError err = vault->encrypt(secret, &encrypted);
		if (err == localvault::VaultError::None) {
			file.write(encrypted.data(), encrypted.size());
			ret = true;
		}
		file.close();
	}
	
	return ret;
}



