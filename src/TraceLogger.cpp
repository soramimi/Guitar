#include "TraceLogger.h"
#include "ApplicationGlobal.h"
#include "common/joinpath.h"
#include "common/jstream.h"
#include <QDebug>

uint64_t TraceLogger::ts()
{
	auto now = std::chrono::steady_clock::now();
	return std::chrono::duration_cast<std::chrono::microseconds>(now - start_time_).count();
}

std::string TraceLogger::escape(const std::string &s)
{
	auto v = jstream::encode_json_string(s);
	return std::string(v.begin(), v.end());
}

void TraceLogger::write(const Item &item, bool comma)
{
	if (!file_.isOpen()) return;

	std::string str = R"({"name":")" + escape(item.name) + R"(","cat":")" + escape(item.category) +
					  R"(","ph":")" + item.phase + R"(","ts":)" + std::to_string(item.timestamp) +
					  R"(,"pid":)" + std::to_string(item.pid) +
					  R"(,"tid":)" + std::to_string(item.tid) + "}";
	if (comma) {
		str += ',';
	}
	str += '\n';
	file_.write(str.c_str(), str.size());
}

TraceLogger::TraceLogger()
{
	start_time_ = std::chrono::steady_clock::now();
}

TraceLogger::~TraceLogger()
{
	close();
}

void TraceLogger::open()
{
	file_.setFileName(global->log_dir / "trace.log");
	if (!file_.open(QFile::WriteOnly)) {
		qWarning() << "Failed to open trace log file:" << file_.fileName();
		return;
	}
	std::string str = R"({"traceEvents":[
)";
	file_.write(str.c_str(), str.size());
	Item item;
	item.name = "Application";
	item.category = "";
	item.phase = 'B';
	item.timestamp = ts();
	item.pid = 1;
	item.tid = 1;
	write(item, true);
}

void TraceLogger::close()
{
	if (file_.isOpen()) {
		Item item;
		item.name = "Application";
		item.category = "";
		item.phase = 'E';
		item.timestamp = ts();
		item.pid = 1;
		item.tid = 1;
		write(item, false);
		std::string str = R"(],
"displayTimeUnit":"ms"
}
)";
		file_.write(str.c_str(), str.size());
	}
	file_.close();
}
