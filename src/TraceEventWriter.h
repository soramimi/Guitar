#ifndef TRACEEVENTWRITER_H
#define TRACEEVENTWRITER_H

#include <chrono>
#include <QFile>
#include <mutex>

class TraceEventWriter {
public:
	struct Event {
		std::string name;
		std::string category;
		char phase = 0;
		uint64_t timestamp;
		int64_t duration = 0; // only for complete events
		int32_t pid;
		int32_t tid;
		std::string args_comment;
	};
	enum Phase {
		PHASE_BEGIN = 'B',
		PHASE_END = 'E',
		PHASE_COUNTER = 'C',
		PHASE_COMPLETE = 'X',
	};
private:
	std::mutex mutex_;
	QFile file_;
	std::chrono::steady_clock::time_point start_time_;
	uint64_t ts();
	std::string escape(std::string const &s);
	void write(Event const &item, bool comma);
public:
	TraceEventWriter();
	~TraceEventWriter();
        void open(const QString &dir);
	void close();
	void put(Event event);
};

#endif // TRACEEVENTWRITER_H
