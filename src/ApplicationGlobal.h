#ifndef APPLICATIONGLOBAL_H
#define APPLICATIONGLOBAL_H

#include <QColor>
#include <QString>
#include <filetype/filetype.h>
#include "AvatarLoader.h"
#include "Theme.h"
#include "main.h"
#include "webclient.h"
#include "common/misc.h"

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

struct ApplicationGlobal {
	MainWindow *mainwindow = nullptr;
	bool start_with_shift_key = false;
	QString organization_name;
	QString application_name;
	QString language_id;
	QString theme_id;
	QString generic_config_dir;
	QString app_config_dir;
	QString config_file_path;
	QColor panel_bg_color;
	ThemePtr theme;

	FileType filetype;

	ApplicationSettings appsettings;

	WebContext webcx = {WebClient::HTTP_1_0};
	AvatarLoader avatar_loader;

	std::vector<AccountProfile> account_profiles;

	void init(QApplication *a);
};

extern ApplicationGlobal *global;

#define PATH_PREFIX "*"

#endif // APPLICATIONGLOBAL_H
