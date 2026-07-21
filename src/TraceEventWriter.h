
#ifndef TRACEEVENTWRITER_H
#define TRACEEVENTWRITER_H

#include <QFile>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

struct TraceEventItem;

class TraceEventWriter {
public:
	enum Phase {
		PHASE_BEGIN = 'B',
		PHASE_END = 'E',
		PHASE_COUNTER = 'C',
		PHASE_COMPLETE = 'X',
	};
private:
	std::mutex mutex_;
	std::thread thread_;
	std::condition_variable cv_;
	bool interrupted_ = false;
	std::deque<std::shared_ptr<TraceEventItem>> queue_;
	QFile file_;
	std::chrono::steady_clock::time_point start_time_;
	uint64_t ts();
	std::string escape(std::string const &s);
	void write(TraceEventItem const &item, bool comma);
public:
	TraceEventWriter();
	~TraceEventWriter();
	void open(const QString &dir);
	void close();
	void put(const TraceEventItem &event);
};

#endif // TRACEEVENTWRITER_H
