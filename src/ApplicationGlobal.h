#ifndef APPLICATIONGLOBAL_H
#define APPLICATIONGLOBAL_H

#include "ApplicationSettings.h"
#include "AvatarLoader.h"
#include "Theme.h"
#include "common/misc.h"
#include "filetype/src/FileType.h"
#include "Git.h"
#include "webclient.h"
#include "MeCaSearch.h"
#include <QColor>
#include <QString>
#include "GenerativeAI.h"
#include "TraceEventWriter.h"
#include "curlclient.h"

class MainWindow;
class IncrementalSearch;

struct AccountProfile {
	QString email;
	QString name;
	AccountProfile() = default;
	AccountProfile(QString const &email, QString const &name)
		: email(email)
		, name(name)
	{
	}
	QString text() const
	{
		return QString("%1 <%2>").arg(name).arg(email);
	}
	operator bool () const
	{
		return misc::isValidMailAddress(email);
	}
};

class ApplicationGlobal : public ApplicationBasicData {
private:
	struct Private;
	Private *m;
public:
	ApplicationGlobal();
	~ApplicationGlobal();

	AbstractGitSession::Option gitopt;
	MainWindow *mainwindow = nullptr;
	bool start_with_shift_key = false;
	QString language_id;
	QString theme_id;
	QString profiles_xml_path;
	QColor panel_bg_color;
	ThemePtr theme;

#ifdef UNSAFE_ENABLED
	bool unsafe_enabled = false;
#endif

	struct Graphics {
		QIcon repository_icon;
		QIcon folder_icon;
		QIcon signature_good_icon;
		QIcon signature_dubious_icon;
		QIcon signature_bad_icon;
		QPixmap transparent_pixmap;
		QPixmap small_digits;
	};
	std::unique_ptr<Graphics> graphics;

	GitContext gcx();

	FileType filetype;

	ApplicationSettings appsettings;

#ifdef USE_LIBCURL
	CurlContext curlcx;
#else
	WebContext webcx = {WebClient::HTTP_1_0};
#endif
	AvatarLoader avatar_loader;

	std::vector<AccountProfile> account_profiles;

	void init1();
	void init2();

	// bool remote_log_enabled = false;
	void open_remote_logger();
	void close_remote_logger();
	void send_remote_logger(std::string const &msg, const char *file = nullptr, int line = 0);

	// bool trace_log_enabled = false;
	void open_trace_logger();
	void close_trace_logger();
	void restart_trace_logger();
	void put_trace_event(const TraceEventWriter::Event &event);

	void writeLog(const std::string_view &str);
	void writeLog(const QString &str);

	std::shared_ptr<AbstractInetClient> inet_client();

	IncrementalSearch *incremental_search();
	MeCaSearch meca;
	QString incremental_search_text;

	GenerativeAI::Credential get_ai_credential(GenerativeAI::AI provider);

	std::string determineFileType(const char *data, size_t size);
	std::string determineFileType(QByteArray const &in)
	{
		return determineFileType(in.data(), in.size());
	}
	std::string determineFileType(std::vector<char> const &in)
	{
		return determineFileType(in.data(), in.size());
	}
	std::string determineFileType(QString const &path);
	std::string determineFileType(std::string const &path);

	static bool isMainThread();

	bool isUnsafeEnabled() const
	{
#ifdef UNSAFE_ENABLED
		return unsafe_enabled;
#else
		return false;
#endif
	}

	bool isAiEnabled() const
	{
		return appsettings.generate_commit_message_by_ai;
	}
};

void GlobalSetOverrideWaitCursor();
void GlobalRestoreOverrideCursor();


#define ASSERT_MAIN_THREAD() Q_ASSERT(ApplicationGlobal::isMainThread())

extern ApplicationGlobal *global;

#endif // APPLICATIONGLOBAL_H
