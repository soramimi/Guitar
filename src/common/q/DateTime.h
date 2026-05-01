#ifndef DATETIME_H
#define DATETIME_H

#include <ctime>

class DateTime;

class Date {
private:
	int year_ = 0;
	int month_ = 0;
	int day_ = 0;
public:
	Date() = default;
	Date(int y, int m, int d) : year_(y), month_(m), day_(d) {}
	int year() const { return year_; }
	int month() const { return month_; }
	int day() const { return day_; }
};

class Time {
private:
	int hour_ = 0;
	int minute_ = 0;
	int second_ = 0;
public:
	Time() = default;
	Time(int h, int m, int s) : hour_(h), minute_(m), second_(s) {}
	int hour() const { return hour_; }
	int minute() const { return minute_; }
	int second() const { return second_; }
};

class DateTime {
private:
	Date date_;
	Time time_;
public:
	DateTime() {}
	DateTime(Date const &date, Time const &time)
		: date_(date), time_(time)
	{
	}
	void setDate(Date const &date)
	{
		date_ = date;
	}
	void setTime(Time const &time)
	{
		time_ = time;
	}

	time_t toSecsSinceEpoch() const
	{
		struct tm t;
		t.tm_year = date_.year() - 1900;
		t.tm_mon = date_.month() - 1;
		t.tm_mday = date_.day();
		t.tm_hour = time_.hour();
		t.tm_min = time_.minute();
		t.tm_sec = time_.second();
		t.tm_isdst = -1;
		return mktime(&t);
	}
};


#endif // DATETIME_H
