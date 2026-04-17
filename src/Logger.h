#ifndef LOGGER_H
#define LOGGER_H

#include <chrono>
#include <string>

class Logger {
private:
	struct Private;
	Private *m;

	struct LogItem;
	typedef std::chrono::system_clock::time_point time_point_t;

	int log_file_permission() const
	{
		return 0644;
	}

	int log_rotate_size() const
	{
		// return 10 * 1024 * 1024;
		return 0; // rotate disabled
	}

	time_point_t now();
	void write(const char *ptr, size_t len);
	void write(const LogItem &item);
	void push(const LogItem &item);
	void rotate();
	void x_close();
	void x_start();
	void x_stop();
	void x_pause(bool f);
	void x_open(const std::string &log_file);
public:
	Logger();
	~Logger();
	void x_logprint(const char *file, int line, int level, std::string_view str);
	void x_logprintf(const char *file, int line, int level, const char *fmt, ...);
	static void pause(bool f);
	static void start();
	static void stop();
	static void open(std::string const &log_file);

};

extern Logger x_logger;

#define LOG_RAW 0
#define LOG_DEFAULT 0x0001
#define LOG_STDERR 0x0002
#define LOG_BOTH (LOG_DEFAULT | LOG_STDERR)
#define logprint(LEVEL, STR) x_logger.x_logprint(__FILE__, __LINE__, LEVEL, STR)
#define logprintf(LEVEL, FMT, ...) x_logger.x_logprintf(__FILE__, __LINE__, LEVEL, FMT, ##__VA_ARGS__)

#endif // LOGGER_H
