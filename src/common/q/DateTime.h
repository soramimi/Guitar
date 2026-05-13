#ifndef DATETIME_H
#define DATETIME_H

#include <ctime>
#include <string>
#include <cstdint>

class DateTime;

class Date {
private:
	bool valid_ = false;
	int year_ = 0;
	int month_ = 0;
	int day_ = 0;
public:
	Date() = default;
	Date(int y, int m, int d)
		: valid_(true)
		, year_(y)
		, month_(m)
		, day_(d)
	{}
	int year() const { return year_; }
	int month() const { return month_; }
	int day() const { return day_; }
	bool isValid() const { return valid_; }
};

class Time {
private:
	bool valid_ = false;
	int hour_ = 0;
	int minute_ = 0;
	int second_ = 0;
public:
	Time() = default;
	Time(int h, int m, int s)
		: valid_(true)
		, hour_(h)
		, minute_(m)
		, second_(s)
	{}
	int hour() const { return hour_; }
	int minute() const { return minute_; }
	int second() const { return second_; }
	bool isValid() const { return valid_; }
};

class DateTime {
private:
	Date date_;
	Time time_;
public:
	DateTime() = default;
	DateTime(Date const &date, Time const &time)
		: date_(date), time_(time)
	{
	}
	Date const &date() const { return date_; }
	Time const &time() const { return time_; }
	void setDate(Date const &date)
	{
		date_ = date;
	}
	void setTime(Time const &time)
	{
		time_ = time;
	}

	bool isValid() const
	{
		return date_.isValid() && time_.isValid();
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

	std::string toString() const;

	static DateTime parseDateTime(char const *s);
	static DateTime fromSecsSinceEpoch(uint64_t t);
};

#endif // DATETIME_H
