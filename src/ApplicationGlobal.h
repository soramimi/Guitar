#ifndef APPLICATIONGLOBAL_H
#define APPLICATIONGLOBAL_H

#include "ApplicationSettings.h"
#include "AvatarLoader.h"
#include "Theme.h"
#include "common/misc.h"
#include "filetype/src/FileType.h"
#include "Git.h"
#include "webclient.h"
#include <QColor>
#include <QString>
#include "GenerativeAI.h"

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

	WebContext webcx = {WebClient::HTTP_1_0};
	AvatarLoader avatar_loader;

	std::vector<AccountProfile> account_profiles;

	void init(QApplication *a);

	void writeLog(const std::string_view &str);
	void writeLog(const QString &str);

	IncrementalSearch *incremental_search();

	GenerativeAI::Credential get_ai_credential(GenerativeAI::AI provider);

	std::string determineFileType(const QByteArray &in);
	// std::string determineFileType(const std::string &path);
	std::string determineFileType(QString const &path);

	static bool isMainThread();

	bool isUnsafeEnabled() const
	{
#ifdef UNSAFE_ENABLED
		return unsafe_enabled;
#else
		return false;
#endif
	}
};

void GlobalSetOverrideWaitCursor();
void GlobalRestoreOverrideCursor();


#define ASSERT_MAIN_THREAD() Q_ASSERT(ApplicationGlobal::isMainThread())

extern ApplicationGlobal *global;

#endif // APPLICATIONGLOBAL_H
