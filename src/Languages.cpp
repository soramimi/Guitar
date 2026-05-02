#include "Languages.h"

Languages::Languages(QObject *parent)
	: QObject(parent)
{
	QString english = tr("English");
	if (english != "English") {
		english += " (English)";
	}
	struct Lang {
		QString description;
		QString code;
	} langs[] = {
		english, "en",
		tr("Spanish"), "es",
		tr("Japanese"), "ja",
		tr("Russian"), "ru",
		tr("Tamil"), "ta",
		tr("Chinese (Simplified)"), "zh-CN",
		tr("Chinese (Traditional/Taiwan)"), "zh-TW",
	};
	for (const auto &lang : langs) {
		addLanguage(lang.code, lang.description); // code first
	}
}


