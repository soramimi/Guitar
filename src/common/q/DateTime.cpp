#include "DateTime.h"
#include <cstdio>
#include <time.h>

std::string DateTime::toString() const
{
	DateTime const &dt = *this;
	if (dt.isValid()) {
		char tmp[100];
		sprintf(tmp, "%04u-%02u-%02u %02u:%02u:%02u"
				, dt.date().year()
				, dt.date().month()
				, dt.date().day()
				, dt.time().hour()
				, dt.time().minute()
				, dt.time().second()
				);
		return tmp;
	}
	return {};
}

DateTime DateTime::parseDateTime(const char *s)
{
	int year, month, day, hour, minute, second;
	if (sscanf(s, "%d-%d-%d %d:%d:%d"
			   , &year
			   , &month
			   , &day
			   , &hour
			   , &minute
			   , &second
			   ) == 6) {
		return DateTime(Date(year, month, day), Time(hour, minute, second));
	}
	return {};
}

DateTime DateTime::fromSecsSinceEpoch(uint64_t t)
{
	struct tm tm;
	_gmtime64_s(&tm, (time_t *)&t);
	Date date(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
	Time time(tm.tm_hour, tm.tm_min, tm.tm_sec);
	return DateTime(date, time);
}
