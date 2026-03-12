#include "Languages.h"

Languages::Languages(QObject *parent)
	: QObject(parent)
{
	struct Lang {
		QString description;
		QString code;
	} langs[] = {
		tr("English"), "en",
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


