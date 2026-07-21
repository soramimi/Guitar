
#ifndef APPLICATIONGLOBAL_H
#define APPLICATIONGLOBAL_H

#include "AbstractGitSession.h"
#include "ApplicationSettings.h"
#include "AvatarLoader.h"
#include "Git.h"
#include <QColor>
#include <QString>
#include <inet/webclient.h>
#include <memory>
#include <subprojects/FileTypePlugin/src/FileType.h>
#include <subprojects/IncrementalSearchPlugin/src/IncrementalSearch.h>

class IncrementalSearch;
class OnePassword;

#ifdef APP_GUITAR
#include "Theme.h"
#endif

class MainWindow;
class QListWidgetItem;
struct TraceEventItem;

struct AccountProfile {
	QString email;
	QString name;
	AccountProfile() = default;
	AccountProfile(QString const &email, QString const &name)
		: email(email)
		, name(name)
	{
	}
	QString text() const;
	explicit operator bool () const;
};

class ApplicationGlobal : public ApplicationBasicData {
private:
	struct Private;
	Private *m;
public:
	ApplicationGlobal();
	~ApplicationGlobal();

	int copyright_year();
	
	char const *product_version();
	
	char const *source_revision();
	
	AbstractGitSession::Option gitopt;
	MainWindow *mainwindow = nullptr;
	bool start_with_shift_key = false;
	QString language_id;
	QString theme_id;
	QString profiles_xml_path;
	QColor panel_bg_color;
#ifdef APP_GUITAR
	ThemePtr theme;
#endif

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

	ApplicationSettings appsettings;

#ifdef USE_LIBCURL
	CurlContext curlcx;
#else
	WebContext webcx = {WebClient::HTTP_1_0};
#endif
	AvatarLoader avatar_loader;

	std::vector<AccountProfile> account_profiles;

	void selftest();

	void open_trace_logger();
	void close_trace_logger();
	void restart_trace_logger();
	void put_trace_event(TraceEventItem const &event);

	void writeLog(const std::string_view &str);
	void writeLog(const QString &str);

	constexpr static std::string_view prefix_chg = "(chg) ";
	constexpr static std::string_view prefix_cpy = "(cpy) ";
	constexpr static std::string_view prefix_ren = "(ren) ";
	constexpr static std::string_view prefix_add = "(add) ";
	constexpr static std::string_view prefix_del = "(del) ";
	constexpr static std::string_view prefix_unmerged = "(unmerged) ";
	// constexpr static std::string_view prefix_empty = "() ";

	std::shared_ptr<AbstractInetClient> inet_client();

	std::shared_ptr<IncrementalSearch> incremental_search;
	IncrementalSearchFilter makeIncrementalSearchFilter(const std::string &filtertext);
	QString incremental_search_text;
	
	std::shared_ptr<FileType> file_type_detector;
	std::string mimetype_by_data(const char *data, size_t size);
	std::string mimetype_by_data(const QByteArray &ba);
	std::string mimetype_by_data(std::vector<char> const &ba);
	std::string mimetype_by_file(const char *path);
	std::string mimetype_by_file(std::string const &path);
	
	std::shared_ptr<OnePassword> onepassword;
	
	GenerativeAI::Credential get_ai_credential(const GenerativeAI::Model &model);

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
		return appsettings.generate_commit_message_with_ai;
	}
};

void GlobalSetOverrideWaitCursor();
void GlobalRestoreOverrideCursor();

QListWidgetItem *new_QListWidgetItem(QString const &text = {});

#define ASSERT_MAIN_THREAD() Q_ASSERT(ApplicationGlobal::isMainThread())

extern ApplicationGlobal *global;

#endif // APPLICATIONGLOBAL_H
