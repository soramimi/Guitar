#ifndef BASICPROCESSWIN_H
#define BASICPROCESSWIN_H

#include "AbstractProcess.h"

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#endif

namespace misc {
std::string get_error_message(uint32_t error_code);
std::string build_command_line(std::vector<std::string> const &args);

}

class _AbstractBasicProcess {
public:
	struct ExecResult {
		bool started = false;
#ifdef _WIN32
		DWORD exit_code = static_cast<DWORD>(-1);
		DWORD error_code = ERROR_SUCCESS;
#else
		uint32_t exit_code = static_cast<uint32_t>(-1);
		uint32_t error_code = 0; // ERROR_SUCCESS;
#endif
		std::string error_message;
	};
	virtual ~_AbstractBasicProcess() { }
	virtual bool start(std::string const &cmd) = 0;
	virtual ExecResult wait() = 0;
	virtual void terminate() = 0;
	virtual void close_input() = 0;
	virtual int write_input(char const *ptr, int n) = 0;
	virtual int read_output(char *ptr, int n) = 0;
	virtual bool is_running() const = 0;
	virtual std::vector<char> const &stdout_bytes() const = 0;
	virtual int get_exit_code() const = 0;
};

class BasicProcessWin : public _AbstractBasicProcess {
private:
	struct Private;
	Private *m;

public:
	struct Options {
		bool no_window = true;
		bool output_stdout = false;
		bool output_vector = false;
		bool output_queue = false;
	};
	BasicProcessWin(Options const &options = Options());
	~BasicProcessWin();
	void set_change_dir(process::helper::dir_string_t const &dir);
	void set_options(Options const &options);
	bool is_running() const;
	bool start(std::string const &cmd);
	ExecResult wait();
	void terminate();
	void close_input();
	int write_input(char const *ptr, int n);
	int read_output(char *ptr, int n);
	int get_exit_code() const;
	std::vector<char> const &stdout_bytes() const;

	bool wait_for_output(std::string const &text);

	// start() 前に設定すること。実行中の変更はスレッドセーフではない。
	void set_completion_callback(std::function<void(bool, std::shared_ptr<void>)> const &fn, std::shared_ptr<void> user_data);
	void notify_completed();
};

#endif // BASICPROCESSWIN_H
