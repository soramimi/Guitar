#ifndef TRACELOGGER_H
#define TRACELOGGER_H

#include <chrono>
#include <QFile>

class TraceLogger {
private:
	QFile file_;
	std::chrono::steady_clock::time_point start_time_;
	uint64_t ts();
	struct Item {
		std::string name;
		std::string category;
		char phase;
		uint64_t timestamp;
		int32_t pid;
		int32_t tid;
	};
	std::string escape(std::string const &s);
	void write(Item const &item, bool comma);
public:
	TraceLogger();
	~TraceLogger();
	void open();
	void close();
};

#endif // TRACELOGGER_H
