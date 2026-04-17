
#include "Logger.h"
#include <cstdio>
#include <cctype>
#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include <ctime>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <string>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

Logger x_logger;

struct Logger::LogItem {
	Logger::time_point_t tp = {};
	int level = 0;
	std::string message;
	std::string file;
	int line = 0;
};

struct Logger::Private {
	std::string log_file;
	bool paused = false;
	int fd_log = -1;
	std::vector<Logger::LogItem> items;
	std::thread thread;
	std::mutex mutex;
	std::condition_variable cv;
	bool interrupted = false;
};

Logger::Logger()
	: m(new Private)
{
}

Logger::~Logger()
{
	x_stop();
	delete m;
}

Logger::time_point_t Logger::now()
{
	return std::chrono::system_clock::now();
}

void Logger::write(char const *ptr, size_t len)
{
	if (m->fd_log != -1) {
		::write(m->fd_log, ptr, len);
	} else {
		::write(fileno(stderr), ptr, len);
	}
}

void Logger::write(LogItem const &item)
{
	const bool FILELINE = false; // set to true to enable file/line logging

	if (item.level == LOG_RAW) {
		write(item.message.c_str(), item.message.size());
	} else {
		auto dur = item.tp.time_since_epoch();
		auto msec = std::chrono::duration_cast<std::chrono::milliseconds>(dur).count();
		time_t t = msec / 1000;
		struct tm *tm = localtime(&t);
		char *text = nullptr;
		std::string msg = item.message;
		if (FILELINE) {
			char buf[100];
			sprintf(buf, "(%d)", item.line);
			msg += " @@";
			msg += item.file;
			msg += buf;
		}
		int len = asprintf(&text, "[%d-%02d-%02d,%02d:%02d:%02d.%03d] %s\n"
				, tm->tm_year + 1900
				, tm->tm_mon + 1
				, tm->tm_mday
				, tm->tm_hour
				, tm->tm_min
				, tm->tm_sec
				, int(msec % 1000)
				, msg.c_str());
		if (text) {
			if (item.level & LOG_DEFAULT) {
				write(text, len);
			}
			if (item.level & LOG_STDERR) {
				fwrite(text, 1, len, stderr);
			}
			free(text);
		}
	}
}

void Logger::rotate()
{
	if (m->fd_log == -1) return;
	
	const int N = 9;
	int i = N;
	std::string dst = m->log_file + '.' + std::to_string(i);
	unlink(dst.c_str());
	while (i > 0) {
		i--;
		std::string src = m->log_file + '.' + std::to_string(i);
		rename(src.c_str(), dst.c_str());
		dst = src;
	}

	int fd_dst = ::open(dst.c_str(), O_WRONLY | O_CREAT | O_TRUNC, log_file_permission());
	if (fd_dst != -1) {
		lseek(m->fd_log, 0, SEEK_SET);
		while (1) {
			char buf[4096];
			int n = read(m->fd_log, buf, sizeof(buf));
			if (n < 1) break;
			if (::write(fd_dst, buf, n) != n) break;
		}
		::close(fd_dst);
	}
	lseek(m->fd_log, 0, SEEK_SET);
	ftruncate(m->fd_log, 0);
}

void Logger::x_open(std::string const &log_file)
{
	x_close();
	m->log_file = log_file;
	m->fd_log = ::open(m->log_file.c_str(), O_RDWR | O_CREAT | O_APPEND, log_file_permission());
	if (m->fd_log == -1) {
		fprintf(stderr, "failed to open log file: %s", m->log_file.c_str());
	}
}

void Logger::x_close()
{
	if (m->fd_log != -1) {
		close(m->fd_log);
		m->fd_log = -1;
	}
}

void Logger::x_start()
{
	m->paused = false;
	m->interrupted = false;

	m->thread = std::thread([this](){
		while (1) {
			std::vector<LogItem> items;
			{
				std::unique_lock lock(m->mutex);
				if (m->items.empty()) {
					if (m->interrupted) break;
					m->cv.wait(lock);
				}
				if (m->paused) continue;
				std::swap(items, m->items);
			}
			if (log_rotate_size() > 0) {
				struct stat st;
				if (fstat(m->fd_log, &st) == 0 && st.st_size >= log_rotate_size()) {
					rotate();
				}
			}
			for (LogItem const &item : items) {
				write(item);
			}
		}
	});
}

void Logger::x_stop()
{
	m->paused = false;
	m->interrupted = true;
	m->cv.notify_all();
	if (m->thread.joinable()) {
		m->thread.join();
	}
	x_close();
	m->interrupted = false;
}

void Logger::x_pause(bool f)
{
	std::lock_guard lock(m->mutex);
	m->paused = f;
	if (!f) {
		m->cv.notify_all();
	}
}

void Logger::start()
{
	x_logger.x_start();
}

void Logger::stop()
{
	x_logger.x_stop();
}

void Logger::open(const std::string &log_file)
{
	x_logger.x_open(log_file);
}

void Logger::push(Logger::LogItem const &item)
{
	std::lock_guard lock(m->mutex);
	m->items.push_back(item);
}

void Logger::x_logprint(const char *file, int line, int level, std::string_view str)
{
	LogItem item;
	item.tp = now();
	item.level = level;
	item.file = file ? file : std::string();
	item.line = line;
	size_t len = str.size();
	if (level != LOG_RAW) {
		while (isspace((unsigned char)str[len - 1])) len--;
		str = str.substr(0, len);
	}
	item.message = std::string(str);
	push(item);
	m->cv.notify_all();
}

void Logger::x_logprintf(char const *file, int line, int level, char const *fmt, ...)
{
	std::string text;

	va_list ap;
	va_start(ap, fmt);
	char *msg = nullptr;
	int len = vasprintf(&msg, fmt, ap);
	if (msg) {
		text.assign(msg, len);
		free(msg);
	}
	va_end(ap);

	x_logprint(file, line, level, text);
}

void Logger::pause(bool f)
{
	x_logger.x_pause(f);
}


