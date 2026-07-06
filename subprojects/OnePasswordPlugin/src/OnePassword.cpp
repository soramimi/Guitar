
#include "OnePassword.h"
#include "libapikey.h"
#include <stdio.h>
#include <string.h>

OnePassword::OnePassword()
{
}

OnePassword::~OnePassword()
{
}

bool OnePassword::open()
{
	return true;
}

QString OnePassword::getapikey(QString const &account_name, QString const &secret_ref_uri)
{
	QString ret;
	char *s = GetAPIKEY((char *)account_name.toStdString().c_str(), (char *)secret_ref_uri.toStdString().c_str());
	if (s) {
		ret = s;
		memset(s, 0, strlen(s));
		FreeString(s);
	}
	return ret;
}

