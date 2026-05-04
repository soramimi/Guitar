
#include "AbstractProcess.h"

#include <stdio.h>
#include "main.h"
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <QElapsedTimer>
#include "common/misc.h"

#ifdef _WIN32
#include <QApplication>
#include <windows.h>
#include <winpty.h>
#else
#include <unistd.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <pty.h>
#endif

#ifndef _WIN32
#include "unix/UnixProcess.h"

struct ProcessResult {
	bool ok = false;
	int exit_code = 0;

	operator bool () const
	{
		return ok;
	}
};

<<<<<<< HEAD
class ProcessPosix : public AbstractProcess {
=======
class PosixProcess : public AbstractProcess {
>>>>>>> a
private:
	std::deque<char> stdout_queue_;
	std::deque<char> stderr_queue_;
	std::vector<char> stdout_vector_;
	std::vector<char> stderr_vector_;
	std::thread thread_;
	mutable std::mutex mutex_;
	std::condition_variable cond_;
	bool input_ready_ = false;
	int input_fd_ = -1;
	int exit_code_ = 128;

	void writeStdOut(char const *buf, size_t len)
	{
		stdout_queue_.insert(stdout_queue_.end(), buf, buf + len);
		stdout_vector_.insert(stdout_vector_.end(), buf, buf + len);
	}
	void writeStdErr(char const *buf, size_t len)
	{
		stderr_queue_.insert(stderr_queue_.end(), buf, buf + len);
		stderr_vector_.insert(stderr_vector_.end(), buf, buf + len);
	}

	std::optional<ProcessResult> run(const std::string &cmd, bool use_input)
	{
		int pipe_stdout_fd[2];
		if (pipe(pipe_stdout_fd) == -1) {
			return std::nullopt;
		}

		int pipe_stderr_fd[2];
		if (pipe(pipe_stderr_fd) == -1) {
			close(pipe_stdout_fd[0]);
			close(pipe_stdout_fd[1]);
			return std::nullopt;
		}

		int pipe_stdin_fd[2];
		if (pipe(pipe_stdin_fd) == -1) {
			close(pipe_stdout_fd[0]);
			close(pipe_stdout_fd[1]);
			close(pipe_stderr_fd[0]);
			close(pipe_stderr_fd[1]);
			return std::nullopt;
		}

		pid_t pid = fork();
		if (pid == -1) {
			close(pipe_stdout_fd[0]);
			close(pipe_stdout_fd[1]);
			close(pipe_stderr_fd[0]);
			close(pipe_stderr_fd[1]);
			close(pipe_stdin_fd[0]);
			close(pipe_stdin_fd[1]);
			return std::nullopt;
		}

		if (pid == 0) {
			// child
			close(pipe_stdout_fd[0]);
			dup2(pipe_stdout_fd[1], STDOUT_FILENO);
			close(pipe_stdout_fd[1]);

			close(pipe_stderr_fd[0]);
			dup2(pipe_stderr_fd[1], STDERR_FILENO);
			close(pipe_stderr_fd[1]);

			close(pipe_stdin_fd[1]);
			dup2(pipe_stdin_fd[0], STDIN_FILENO);
			close(pipe_stdin_fd[0]);

			std::vector<char *> argv;
			std::vector<std::string> args;
			UnixProcess::parseArgs(cmd, &args);
			for (std::string &s : args) {
				argv.push_back(s.data());
			}
			argv.push_back(nullptr);

			if (!argv.empty()) {
				execvp(argv[0], argv.data());
			}
			_exit(1);
		}

		// parent
		close(pipe_stdout_fd[1]);
		close(pipe_stderr_fd[1]);
		close(pipe_stdin_fd[0]);

		{
			std::lock_guard<std::mutex> lock(mutex_);
			input_fd_ = pipe_stdin_fd[1];
			input_ready_ = true;
		}
		cond_.notify_all();

		ProcessResult ret;

		if (!use_input) {
			closeInput(true);
		}

		int stdout_rd = pipe_stdout_fd[0];
		int stderr_rd = pipe_stderr_fd[0];

		while (stdout_rd != -1 || stderr_rd != -1) {
			fd_set fds;
			FD_ZERO(&fds);
			int nfds = 0;
			if (stdout_rd != -1) {
				FD_SET(stdout_rd, &fds);
				if (stdout_rd >= nfds) nfds = stdout_rd + 1;
			}
			if (stderr_rd != -1) {
				FD_SET(stderr_rd, &fds);
				if (stderr_rd >= nfds) nfds = stderr_rd + 1;
			}

			if (select(nfds, &fds, nullptr, nullptr, nullptr) <= 0) break;

			char buf[256];
			if (stdout_rd != -1 && FD_ISSET(stdout_rd, &fds)) {
				ssize_t n = read(stdout_rd, buf, sizeof(buf));
				if (n > 0) {
					std::lock_guard<std::mutex> lock(mutex_);
					writeStdOut(buf, (size_t)n);
				} else {
					close(stdout_rd);
					stdout_rd = -1;
				}
			}
			if (stderr_rd != -1 && FD_ISSET(stderr_rd, &fds)) {
				ssize_t n = read(stderr_rd, buf, sizeof(buf));
				if (n > 0) {
					std::lock_guard<std::mutex> lock(mutex_);
					writeStdErr(buf, (size_t)n);
				} else {
					close(stderr_rd);
					stderr_rd = -1;
				}
			}
		}

		int status = 0;
		waitpid(pid, &status, 0);
		if (WIFEXITED(status)) {
			ret.exit_code = WEXITSTATUS(status);
		}

		return ret;
	}
public:
<<<<<<< HEAD
	~ProcessPosix()
=======
	~PosixProcess()
>>>>>>> a
	{
		closeInput(true);
		if (thread_.joinable()) {
			thread_.join();
		}
	}
	bool isRunning() const
	{
		return thread_.joinable();
	}
	void writeInput(const char *ptr, int len) override
	{
		::write(input_fd_, ptr, len);
	}
	int readOutput(char *ptr, int len)
	{
		int n = stdout_queue_.size();
		if (n > len) n = len;
		for (int i = 0; i < n; i++) {
			ptr[i] = stdout_queue_.front();
			stdout_queue_.pop_front();
		}
		return n;
	}
	void start(const std::string &command, bool use_input) override
	{
		thread_ = std::thread([&](){
			auto ret = run(command, use_input);
			if (ret) {
				exit_code_ = ret->exit_code;
			} else {
				exit_code_ = 128;
			}
		});
		{
			std::unique_lock<std::mutex> lock(mutex_);
			cond_.wait(lock, [this]{ return input_ready_; });
		}
	}
	int wait() override
	{
		(void)time;
		closeInput(true);
		if (thread_.joinable()) {
			thread_.join();
			return exit_code_;
		}
		return 128;
	}
	void stop() override
	{
		wait();
	}
	int getExitCode() const override
	{
		return exit_code_;
	}
	void readResult(std::vector<char> *out)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		out->insert(out->end(), stdout_queue_.begin(), stdout_queue_.end());
		stdout_queue_.clear();
	}

	std::string outstring() const
	{
		std::string ret;
		{
			std::lock_guard<std::mutex> lock(mutex_);
			ret.insert(ret.end(), stdout_vector_.begin(), stdout_vector_.end());
		}
		return ret;
	}
	std::string errstring() const
	{
		std::string ret;
		{
			std::lock_guard<std::mutex> lock(mutex_);
			ret.insert(ret.end(), stderr_vector_.begin(), stderr_vector_.end());
		}
		return ret;
	}

	void closeInput(bool justnow)
	{
		if (input_fd_ != -1) {
			close(input_fd_);
			input_fd_ = -1;
		}
	}
};

<<<<<<< HEAD
class ProcessPosixPty : public AbstractPtyProcess {
=======
class PosixPtyProcess : public AbstractPtyProcess {
>>>>>>> a
private:
	std::deque<char> stdout_queue_;
	std::deque<char> stderr_queue_;
	std::vector<char> stdout_vector_;
	std::vector<char> stderr_vector_;
	std::thread thread_;
	mutable std::mutex mutex_;
	std::condition_variable cond_;
	bool input_ready_ = false;
	int input_fd_ = -1;
	int exit_code_ = 128;

	void writeStdOut(char const *buf, size_t len)
	{
		stdout_queue_.insert(stdout_queue_.end(), buf, buf + len);
		stdout_vector_.insert(stdout_vector_.end(), buf, buf + len);
	}
	void writeStdErr(char const *buf, size_t len)
	{
		stderr_queue_.insert(stderr_queue_.end(), buf, buf + len);
		stderr_vector_.insert(stderr_vector_.end(), buf, buf + len);
	}

	bool exec_posixpty(std::string const &cmd)
	{
		std::string ret;

		int master_fd;
		pid_t pid = forkpty(&master_fd, nullptr, nullptr, nullptr);
		if (pid == -1) {
			return false;
		}

		if (pid == 0) {
			// child

			std::vector<char *> argv;
			std::vector<std::string> args;
			UnixProcess::parseArgs(cmd, &args);
			for (std::string &s : args) {
				argv.push_back(s.data());
			}
			argv.push_back(nullptr);

			if (!argv.empty()) {
				execvp(argv[0], argv.data());
			}
			_exit(1);
		}

		// parent
		{
			std::lock_guard<std::mutex> lock(mutex_);
			input_fd_ = master_fd;
			input_ready_ = true;
		}
		cond_.notify_all();

		char buf[256];
		ssize_t n;
		while ((n = read(master_fd, buf, sizeof(buf))) > 0) {
			std::lock_guard<std::mutex> lock(mutex_);
			writeStdOut(buf, (size_t)n);
		}
		close(master_fd);
		{
			std::lock_guard<std::mutex> lock(mutex_);
			input_fd_ = -1;
		}

		waitpid(pid, nullptr, 0);

		if (!stderr_vector_.empty() && stderr_vector_.back() == '\n') stderr_vector_.pop_back();
		if (!stderr_vector_.empty() && stderr_vector_.back() == '\r') stderr_vector_.pop_back();

		return true;
	}
public:
	bool isRunning() const
	{
		return thread_.joinable();
	}
	void writeInput(const char *ptr, int len) override
	{
		int fd;
		{
			std::lock_guard<std::mutex> lock(mutex_);
			fd = input_fd_;
		}
		if (fd != -1) {
			::write(fd, ptr, len);
		}
	}
	void closeInput()
	{
		int fd;
		{
			std::lock_guard<std::mutex> lock(mutex_);
			fd = input_fd_;
		}
		if (fd != -1) {
			char eof_char = 0x04; // Ctrl+D: PTY canonical mode EOF
			::write(fd, &eof_char, 1);
		}
	}
	int readOutput(char *ptr, int len)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		int n = stdout_queue_.size();
		if (n > len) n = len;
		for (int i = 0; i < n; i++) {
			ptr[i] = stdout_queue_.front();
			stdout_queue_.pop_front();
		}
		return n;
	}
	void start(const std::string &cmd, const std::string &env) override
	{
		thread_ = std::thread([this, cmd](){
			if (exec_posixpty(cmd)) {

			}
		});
		{
			std::unique_lock<std::mutex> lock(mutex_);
			cond_.wait(lock, [this]{ return input_ready_; });
		}
	}
	bool wait(unsigned long time = ULONG_MAX) override
	{
		(void)time;
		closeInput();
		if (thread_.joinable()) {
			thread_.join();
			return true;
		}
		return true;
	}
	void stop() override
	{
		wait();
	}
	int getExitCode() const
	{
		return 0;
	}
	void readResult(std::vector<char> *out)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		out->insert(out->end(), stdout_queue_.begin(), stdout_queue_.end());
		stdout_queue_.clear();
	}

	std::string outstring() const
	{
		std::string ret;
		{
			std::lock_guard<std::mutex> lock(mutex_);
			ret.insert(ret.end(), stdout_vector_.begin(), stdout_vector_.end());
		}
		return ret;
	}
	std::string errstring() const
	{
		std::string ret;
		{
			std::lock_guard<std::mutex> lock(mutex_);
			ret.insert(ret.end(), stderr_vector_.begin(), stderr_vector_.end());
		}
		return ret;
	}
};



#else

std::wstring convert_str_to_wstr(std::string const &str)
{
	std::wstring wstr;
	if (str.empty()) return wstr;
	int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), nullptr, 0);
	if (len > 0) {
		wstr.resize(len);
		MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstr[0], len);
	}
	return wstr;
}

class ProcessWin : public AbstractProcess {
private:
	std::thread thread_;
	std::mutex mutex_;
	std::condition_variable cond_;
	std::deque<char> output_queue_; // for log
	std::vector<char> output_vector_; // for result
	PROCESS_INFORMATION pi_ = {};
	DWORD exit_code_ = 128;
	void writeOutput(char const *buf, size_t len)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		output_queue_.insert(output_queue_.end(), buf, buf + len);
		output_vector_.insert(output_vector_.end(), buf, buf + len);
	}
	bool exec_win(const std::string &cmd, bool use_input)
	{
		std::string ret;

		HANDLE hReadPipe = nullptr;
		HANDLE hWritePipe = nullptr;
		SECURITY_ATTRIBUTES sa = {};
		sa.nLength = sizeof(sa);
		sa.bInheritHandle = TRUE;
		if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
			return false;
		}
		SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

		STARTUPINFOW si = {};
		si.cb = sizeof(si);
		si.dwFlags = STARTF_USESTDHANDLES;
		si.hStdOutput = hWritePipe;
		si.hStdError = hWritePipe;

		std::wstring wcmd = convert_str_to_wstr(cmd);

		pi_ = {};
		BOOL ok = CreateProcessW(
					  nullptr, wcmd.data(),
					  nullptr, nullptr,
					  TRUE, CREATE_NO_WINDOW,
					  nullptr, nullptr,
					  &si, &pi_
					  );

		if (!use_input) {
			CloseHandle(hWritePipe);
		}

		if (!ok) {
			CloseHandle(hReadPipe);
			return false;
		}

		char buf[256];
		DWORD n;
		while (ReadFile(hReadPipe, buf, sizeof(buf), &n, nullptr) && n > 0) {
			writeOutput(buf, n);
		}
		CloseHandle(hReadPipe);

		WaitForSingleObject(pi_.hProcess, INFINITE);
		CloseHandle(pi_.hProcess);
		CloseHandle(pi_.hThread);

		if (!ret.empty() && ret.back() == '\n') ret.pop_back();
		if (!ret.empty() && ret.back() == '\r') ret.pop_back();

		return true;
	}
public:
	~ProcessWin()
	{
		wait();
	}
	std::string outstring() const
	{
		std::string s;
		for (char c : output_vector_) {
			s.push_back(c);
		}
		return s;
	}
	std::string errstring() const
	{
		return {};
	}
	void start(const std::string &command, bool use_input)
	{
		thread_ = std::thread([&](){
			exec_win(command, use_input);
		});
	}
	int wait()
	{
		if (thread_.joinable()) {
			thread_.join();
			GetExitCodeProcess(pi_.hProcess, (DWORD *)&exit_code_);
		}
		return 0;
	}
	void writeInput(const char *ptr, int len)
	{
	}
	void closeInput(bool justnow)
	{
	}
	void readResult(std::vector<char> *out)
	{
		*out = output_vector_;
		output_vector_.clear();
	}
	void stop()
	{
		wait();
	}
	int getExitCode() const
	{
		return (int)exit_code_;
	}
};


class ProcessWinPty : public AbstractPtyProcess {
private:
	std::thread thread_;
	std::mutex mutex_;
	std::condition_variable cond_;
	int input_fd_ = -1;
	HANDLE hConout_ = INVALID_HANDLE_VALUE;
	HANDLE hInput_ = INVALID_HANDLE_VALUE;
	int exit_code_ = 128;

	std::string exec_winpty(const std::string &cmd, bool use_input)
	{
		std::string ret;
		winpty_error_ptr_t err = nullptr;

		winpty_config_t *cfg = winpty_config_new(WINPTY_FLAG_PLAIN_OUTPUT, &err);
		if (!cfg) {
			winpty_error_free(err);
			return ret;
		}

		winpty_t *wp = winpty_open(cfg, &err);
		winpty_config_free(cfg);
		if (!wp) {
			winpty_error_free(err);
			return ret;
		}

		std::wstring wcmd = convert_str_to_wstr(cmd);

		winpty_spawn_config_t *scfg = winpty_spawn_config_new(
										  WINPTY_SPAWN_FLAG_AUTO_SHUTDOWN,
										  nullptr,
										  wcmd.data(),
										  nullptr,
										  nullptr,
										  &err
										  );
		if (!scfg) {
			winpty_error_free(err);
			winpty_free(wp);
			return ret;
		}

		hConout_ = CreateFileW(
							 winpty_conout_name(wp),
							 GENERIC_READ, 0, nullptr,
							 OPEN_EXISTING, 0, nullptr
							 );

		if (use_input) {
			hInput_ = CreateFileW(
								 winpty_conin_name(wp),
								 GENERIC_WRITE, 0, nullptr,
								 OPEN_EXISTING, 0, nullptr
								 );
		}

		HANDLE hProcess = nullptr;
		DWORD createError = 0;
		BOOL ok = winpty_spawn(wp, scfg, &hProcess, nullptr, &createError, &err);
		winpty_spawn_config_free(scfg);
		if (!ok) {
			winpty_error_free(err);
			winpty_free(wp);
			return ret;
		}



		if (hConout_ != INVALID_HANDLE_VALUE) {
			char buf[256];
			DWORD n;
			while (ReadFile(hConout_, buf, sizeof(buf), &n, nullptr) && n > 0) {
				std::lock_guard<std::mutex> lock(mutex_);
				writeOutput(buf, n);
			}
			CloseHandle(hConout_);
		}

		WaitForSingleObject(hProcess, INFINITE);
		CloseHandle(hProcess);
		winpty_free(wp);

		if (!ret.empty() && ret.back() == '\n') ret.pop_back();
		if (!ret.empty() && ret.back() == '\r') ret.pop_back();

		return ret;
	}
public:
	bool isRunning() const
	{
		return thread_.joinable();
	}
	void writeInput(const char *ptr, int len)
	{
		if (hInput_ != INVALID_HANDLE_VALUE) {
			char const *begin = ptr;
			char const *end = begin + len;
			char const *left = begin;
			char const *right = begin;
			while (1) {
				int c = -1;
				if (right < end) {
					c = *right & 0xff;
				}
				if (c == '\r' || c == '\n' || c < 0) {
					if (left < right) {
						DWORD written;
						WriteFile(hInput_, left, right - left, &written, nullptr);
					}
					if (c < 0) break;
					right++;
					if (c == '\r') {
						if (*right == '\n') {
							right++;
						}
						c = '\r';
					} else if (c == '\n') {
						c = '\r';
					} else {
						c = -1;
					}
					if (c >= 0) {
						DWORD written;
						WriteFile(hInput_, &c, 1, &written, nullptr);
					}
					left = right;
				} else {
					right++;
				}
			}
		}
	}
	int readOutput(char *ptr, int len)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		int n = output_queue_.size();
		if (n > len) n = len;
		for (int i = 0; i < n; i++) {
			ptr[i] = output_queue_.front();
			output_queue_.pop_front();
		}
		return n;
	}
	void start(const std::string &cmd, const std::string &env)
	{
		thread_ = std::thread([&](std::string const &cmd){
			exec_winpty(cmd, true);
		}, cmd);
	}
	bool wait(unsigned long time = ULONG_MAX)
	{
		closeInput();
		if (thread_.joinable()) {
			thread_.join();
			return true;
		}
		return false;
	}
	void stop()
	{
		if (hConout_ != INVALID_HANDLE_VALUE) {
			CloseHandle(hConout_);
			hConout_ = INVALID_HANDLE_VALUE;
		}
		wait();
	}
	int getExitCode() const
	{
		return exit_code_;
	}
	void readResult(std::vector<char> *out)
	{
		*out = output_vector_;
		output_vector_.clear();
	}
	void closeInput()
	{
		if (hInput_ != INVALID_HANDLE_VALUE) {
			CloseHandle(hInput_);
			hInput_ = INVALID_HANDLE_VALUE;
		}
	}
};



static std::vector<char> strip_vt(std::vector<char> const &s)
{
	std::vector<char> out;
	char const *begin = s.data();
	char const *end = begin + s.size();
	char const *ptr = begin;
	while (ptr < end) {
		if (*ptr == 0x1b) {
			ptr++;
			if (ptr < end && *ptr == '[') {
				ptr++;
				while (ptr < end && *ptr >= 0x20 && *ptr <= 0x3f) ptr++;
				if (ptr < end && *ptr >= 0x40 && *ptr <= 0x7e) ptr++;
			} else if (ptr < end && *ptr == ']') {
				ptr++;
				while (ptr < end) {
					if (*ptr == 0x07) { // BEL
						ptr++;
						break;
					}
					if (*ptr == 0x1b && ptr + 1 < end && ptr[1] == '\\') { // ST
						ptr += 2;
						break;
					}
					ptr++;
				}
			} else if (ptr < end) {
				ptr++; // 2-char ESC sequence
			}
		} else {
			out.push_back(*ptr++);
		}
	}
	return out;
}

// wip:
class ProcessWinConPTY : public AbstractPtyProcess {
private:
	DWORD exit_code_ = 0;
	std::vector<char> out_;
	std::thread thread_;
	HANDLE hPipeInWrite_ = nullptr;
	bool exec_win_conpty(const std::string &cmd, const std::string &env, bool use_input)
	{
		const bool interactive = true; // TODO: use_input should not be the same as interactive, but for testing we can keep it simple for now

		std::string ret;

		HANDLE hStdinRead = nullptr;
		HANDLE hStdinWrite = nullptr;
		HANDLE hReadPipe = nullptr;
		HANDLE hWritePipe = nullptr;
		SECURITY_ATTRIBUTES sa = {};
		sa.nLength = sizeof(sa);
		sa.bInheritHandle = TRUE;
		if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
			return false;
		}
		SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);
		if (!interactive) {
			if (!CreatePipe(&hStdinRead, &hStdinWrite, &sa, 0)) {
				CloseHandle(hReadPipe);
				CloseHandle(hWritePipe);
				return false;
			}
			SetHandleInformation(hStdinWrite, HANDLE_FLAG_INHERIT, 0);
		}

		STARTUPINFOW si = {};
		si.cb = sizeof(si);
		si.dwFlags = STARTF_USESTDHANDLES;
		si.hStdInput = interactive ? GetStdHandle(STD_INPUT_HANDLE) : hStdinRead;
		si.hStdOutput = hWritePipe;
		si.hStdError = hWritePipe;

		PROCESS_INFORMATION pi = {};

		std::wstring wcmd = convert_str_to_wstr(cmd);
		BOOL ok = CreateProcessW(
			nullptr, wcmd.data(),
			nullptr, nullptr,
			TRUE, 0,
			nullptr, nullptr,
			&si, &pi
		);
		if (hStdinRead) {
			CloseHandle(hStdinRead);
		}
		CloseHandle(hWritePipe);

		if (!ok) {
			if (hStdinWrite) {
				CloseHandle(hStdinWrite);
			}
			CloseHandle(hReadPipe);
			return false;
		}

		std::thread writer;
		if (!interactive) {
			CloseHandle(hStdinWrite);
		}

		char buf[256];
		DWORD n;
		while (ReadFile(hReadPipe, buf, sizeof(buf), &n, nullptr) && n > 0) {
			// ::write(1, buf, n);
			// ret.append(buf, n);
			writeOutput(buf, n);
		}
		CloseHandle(hReadPipe);

		WaitForSingleObject(pi.hProcess, INFINITE);
		if (writer.joinable()) {
			CancelSynchronousIo(writer.native_handle());
			writer.join();
		}
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);

		while (!ret.empty() && (ret.back() == '\n' || ret.back() == '\r')) {
			ret.pop_back();
		}

		ret = misc::strip_vt(ret);
		printf("%s", ret.c_str());

		return true;
	}
public:
	bool isRunning() const override { return false; }
	void writeInput(const char *ptr, int len) override
	{
		if (hPipeInWrite_ != INVALID_HANDLE_VALUE) {
			DWORD n;
			WriteFile(hPipeInWrite_, ptr, (DWORD)len, &n, nullptr);
		}
	}
	int readOutput(char *ptr, int len) override { return 0; }
	void start(const std::string &cmd, const std::string &env) override
	{
		thread_ = std::thread([&](std::string const &cmd){
			exec_win_conpty(cmd, env, true);
		}, cmd);
	}
	bool wait(unsigned long time = ULONG_MAX) override
	{
		closeInput();
		if (thread_.joinable()) {
			thread_.join();
			return true;
		}
		return false;
	}
	void stop() override
	{
	}
	int getExitCode() const override
	{
		return (int)exit_code_;
	}
	void readResult(std::vector<char> *out) override
	{
		*out = std::move(output_vector_);
	}
	void closeInput()
	{
		if (hPipeInWrite_ != INVALID_HANDLE_VALUE) {
			CloseHandle(hPipeInWrite_);
			hPipeInWrite_ = INVALID_HANDLE_VALUE;
		}
	}
};



bool is_conpty_available()
{
	HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
	if (!hKernel32) return false;
	return GetProcAddress(hKernel32, "CreatePseudoConsole") != nullptr;
}

#endif

#ifdef _WIN32
#include "common/misc.h"
#include "common/base64.h"

constexpr static char const *conpty_agent_tag = "--conpty-agent---";

bool exec_conpty_agent()
{
	std::string cmd = "git --version";
	std::string tag = conpty_agent_tag;
	std::string cmd2 = GetCommandLineA();
	auto it = cmd2.rfind('\\');
	if (it != std::string::npos) {
		cmd2 = cmd2.substr(0, it + 1);
	} else {
		cmd2 = {};
	}
	cmd2 += "\\conpty-agent.exe ";
	cmd2 += tag;
	cmd2 += ' ';
	cmd2 += base64_encode(cmd);
	ProcessWin proc;
	proc.start(cmd2, false);
	proc.wait();
	std::vector<char> out;
	proc.readResult(&out);
	std::string s(out.begin(), out.end());
	fprintf(stderr, "conpty agent output: [%s]\n", s.c_str());
	return true;
}
#endif

// experiment for process execution

void process_test()
{
}

/*

windows process execution time:

|方法|時間 [ms]|備考|
|-|-|-|
|exec_win|50..55||
|exec_winpty|100..150||
|exec_win_conpty|50..55|環境によって出力を取りこぼす不具合|
|via conpty-agent.exe|80..85||

linux process execution time:

|方法|時間 [ms]|備考|
|-|-|-|
|exec_posix|5..15||
|exec_posixpty|5..15||

- 圧倒的にLinuxの方が高速。Windowsはプロセス生成に時間がかかる。

*/




