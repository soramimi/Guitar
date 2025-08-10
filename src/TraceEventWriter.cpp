#include "TraceEventWriter.h"
#include "ApplicationGlobal.h"
#include "common/joinpath.h"
#include "common/jstream.h"
#include <QDebug>
#include <thread>

std::vector<std::thread::id> thread_ids;

int tid()
{
	std::thread::id id = std::this_thread::get_id();
	for (int i = 0; i < thread_ids.size(); i++) {
		if (thread_ids[i] == id) {
			return i;
		}
	}
	int i = (int)thread_ids.size();
	thread_ids.push_back(id);
	return i;
}

uint64_t TraceEventWriter::ts()
{
	auto now = std::chrono::steady_clock::now();
	return std::chrono::duration_cast<std::chrono::microseconds>(now - start_time_).count();
}

std::string TraceEventWriter::escape(const std::string &s)
{
	auto v = jstream::encode_json_string(s);
	return std::string(v.begin(), v.end());
}

void TraceEventWriter::write(const Event &item, bool comma)
{
	if (!file_.isOpen()) return;

	std::string str = R"({"name":")" + escape(item.name) + R"(","cat":")" + escape(item.category) +
					  R"(","ph":")" + item.phase + R"(","ts":)" + std::to_string(item.timestamp) +
					  R"(,"pid":)" + std::to_string(item.pid) +
					  R"(,"tid":)" + std::to_string(item.tid);
	if (!item.args_comment.empty()) {
		str += R"(,"args":{"comment":")" + escape(item.args_comment) + R"("})";
	}
	str += '}';
	if (comma) {
		str += ',';
	}
	str += '\n';
	for (size_t i = 0; i < str.size(); i++) {
		if (str[i] == 0) {
			qDebug() << "---" << Q_FUNC_INFO;
		}

	}
	{
		std::lock_guard lock(mutex_);
		file_.write(str.c_str(), str.size());
		file_.flush();
	}
}

TraceEventWriter::TraceEventWriter()
{
	start_time_ = std::chrono::steady_clock::now();
}

TraceEventWriter::~TraceEventWriter()
{
	close();
}

void TraceEventWriter::open()
{
	file_.setFileName(global->log_dir / "trace.log");
	if (!file_.open(QFile::WriteOnly)) {
		qWarning() << "Failed to open trace log file:" << file_.fileName();
		return;
	}
	std::string str = R"({"traceEvents":[
)";
	file_.write(str.c_str(), str.size());

	Event event;
	event.name = "Application";
	event.phase = PHASE_BEGIN;

	put(event);
}

void TraceEventWriter::close()
{
	if (file_.isOpen()) {
		Event event;
		event.name = "Application";
		event.category = "";
		event.phase = PHASE_END;
		event.timestamp = ts();
		event.pid = 1;
		event.tid = tid();

		write(event, false);
		std::string str = R"(],
"displayTimeUnit":"ms"
}
)";
		file_.write(str.c_str(), str.size());
	}
	file_.close();
}

void TraceEventWriter::put(Event event)
{
	event.timestamp = ts();
	event.pid = 1;
	event.tid = tid();
	write(event, true);
}

