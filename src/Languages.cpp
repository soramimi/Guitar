#include "Languages.h"

Languages::Languages(QObject *parent)
	: QObject(parent)
{
	addLanguage("en"   , tr("English"));
	addLanguage("ja"   , tr("Japanese"));
	addLanguage("ru"   , tr("Russian"));
	addLanguage("es"   , tr("Spanish"));
	addLanguage("zh-CN", tr("Chinese (Simplified)"));
	addLanguage("zh-TW", tr("Chinese (Traditional/Taiwan)"));
}


