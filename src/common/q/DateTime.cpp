#include "DateTime.h"
#include <cstdio>
#include <time.h>

std::string Date::toString() const
{
	if (isValid()) {
		char tmp[100];
		sprintf(tmp, "%04u-%02u-%02u"
				, year()
				, month()
				, day()
				);
		return tmp;
	}
	return {};
}

std::string Time::toString() const
{
	if (isValid()) {
		char tmp[100];
		sprintf(tmp, "%02u:%02u:%02u"
				, hour()
				, minute()
				, second()
				);
		return tmp;
	}
	return {};
}

std::string DateTime::toString() const
{
	DateTime const &dt = *this;
	if (dt.isValid()) {
		return dt.date().toString() + ' ' + dt.time().toString();
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
#ifdef _WIN32
	_gmtime64_s(&tm, (time_t *)&t);
#else
	gmtime_r((time_t *)&t, &tm);
#endif
	Date date(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
	Time time(tm.tm_hour, tm.tm_min, tm.tm_sec);
	return DateTime(date, time);
}
