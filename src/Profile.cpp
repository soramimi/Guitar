#include "Profile.h"
#include <QDebug>
#include "Logger.h"

void Profile::log(const QString &s)
{
	// qDebug() << s;
}

Profile::Profile(const char *func)
	: func_(func)
{
	trace_logger_.begin("function", func);
	logprintf(LOG_DEFAULT, "--- profile [enter] <<%s>>", func);
	timer_.start();
}

Profile::~Profile()
{
	logprintf(LOG_DEFAULT, "--- profile [leave] <<%s>> %d ms", func_.c_str(), (int)timer_.elapsed());
	trace_logger_.end();
}
