#ifndef APPLICATIONGLOBAL_H
#define APPLICATIONGLOBAL_H

#include "ApplicationSettings.h"
#include "AvatarLoader.h"
#include "Theme.h"
#include "common/misc.h"
#include "FileType.h"
#include "webclient.h"
#include <QColor>
#include <QString>

class MainWindow;

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
public:
	MainWindow *mainwindow = nullptr;
	bool start_with_shift_key = false;
	QString language_id;
	QString theme_id;
	QString profiles_xml_path;
	QColor panel_bg_color;
	ThemePtr theme;

	FileType filetype;

	ApplicationSettings appsettings;

	WebContext webcx = {WebClient::HTTP_1_0};
	AvatarLoader avatar_loader;

	std::vector<AccountProfile> account_profiles;

	void init(QApplication *a);

	void writeLog(const char *ptr, int len, bool record) const;
	void writeLog(const std::string_view &str, bool record) const;
	void writeLog(const QString &str, bool record) const;

	QString OpenAiApiKey() const
	{
		if (appsettings.use_openai_api_key_environment_value) {
			return getenv("OPENAI_API_KEY");
		} else {
			return appsettings.openai_api_key;
		}
	}
	QString AnthropicAiApiKey() const
	{
		if (appsettings.use_anthropic_api_key_environment_value) {
			return getenv("ANTHROPIC_API_KEY");
		} else {
			return appsettings.anthropic_api_key;
		}
	}
	QString GoogleApiKey() const
	{
		if (appsettings.use_google_api_key_environment_value) {
			return getenv("GOOGLE_API_KEY");
		} else {
			return appsettings.google_api_key;
		}
	}
	
	static bool isMainThread();
};

#define ASSERT_MAIN_THREAD() Q_ASSERT(ApplicationGlobal::isMainThread())

extern ApplicationGlobal *global;

#endif // APPLICATIONGLOBAL_H
