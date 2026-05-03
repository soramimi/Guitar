
#include "AbstractProcess.h"

#include <stdio.h>
#include "main.h"
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <QElapsedTimer>

#ifdef _WIN32
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

class ProcessPosix : public AbstractProcess {
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
	~ProcessPosix()
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

class ProcessPosixPty : public AbstractPtyProcess {
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
	void writeOutput(char const *buf, size_t len)
	{
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

		PROCESS_INFORMATION pi = {};
		BOOL ok = CreateProcessW(
					  nullptr, wcmd.data(),
					  nullptr, nullptr,
					  TRUE, CREATE_NO_WINDOW,
					  nullptr, nullptr,
					  &si, &pi
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
			// ret.append(buf, n);
			writeOutput(buf, n);
		}
		CloseHandle(hReadPipe);

		WaitForSingleObject(pi.hProcess, INFINITE);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);

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
};


class ProcessWinPty : public AbstractPtyProcess {
private:
	std::thread thread_;
	std::mutex mutex_;
	std::condition_variable cond_;
	int input_fd_ = -1;
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

		HANDLE hConout = CreateFileW(
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



		if (hConout != INVALID_HANDLE_VALUE) {
			char buf[256];
			DWORD n;
			while (ReadFile(hConout, buf, sizeof(buf), &n, nullptr) && n > 0) {
				// ret.append(buf, n);
				writeOutput(buf, n);
			}
			CloseHandle(hConout);
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
			DWORD n;
			WriteFile(hInput_, ptr, (DWORD)len, &n, nullptr);
		}
	}
	int readOutput(char *ptr, int len)
	{
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

		HANDLE hPipeInRead = nullptr;
		HANDLE hPipeOutRead = nullptr;
		HANDLE hPipeOutWrite = nullptr;

		if (!CreatePipe(&hPipeInRead, &hPipeInWrite_, nullptr, 0)) {
			return {};
		}
		if (!CreatePipe(&hPipeOutRead, &hPipeOutWrite, nullptr, 0)) {
			CloseHandle(hPipeInRead);
			CloseHandle(hPipeInWrite_);
			return {};
		}

		HPCON hPC = nullptr;
		COORD size = {80, 25};
		HRESULT hr = CreatePseudoConsole(size, hPipeInRead, hPipeOutWrite, 0, &hPC);
		// ConPTY が両端を引き継ぐので呼び出し側のコピーを閉じる
		CloseHandle(hPipeInRead);
		CloseHandle(hPipeOutWrite);
		if (FAILED(hr)) {
			CloseHandle(hPipeInWrite_);
			CloseHandle(hPipeOutRead);
			return {};
		}

		SIZE_T attrSize = 0;
		InitializeProcThreadAttributeList(nullptr, 1, 0, &attrSize);

		STARTUPINFOEXW siEx = {};
		siEx.StartupInfo.cb = sizeof(siEx);
		siEx.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), 0, attrSize);
		if (!siEx.lpAttributeList
			|| !InitializeProcThreadAttributeList(siEx.lpAttributeList, 1, 0, &attrSize)
			|| !UpdateProcThreadAttribute(siEx.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, hPC, sizeof(hPC), nullptr, nullptr)) {
			if (siEx.lpAttributeList) HeapFree(GetProcessHeap(), 0, siEx.lpAttributeList);
			CloseHandle(hPipeInWrite_);
			CloseHandle(hPipeOutRead);
			ClosePseudoConsole(hPC);
			return {};
		}

		// std::wstring wcmd = L"git --version";
		std::wstring wcmd = convert_str_to_wstr(cmd);

		std::vector<wchar_t> envbuf;
		if (!env.empty()) {
			envbuf.resize(env.size() + 2);
			std::wstring wenv = convert_str_to_wstr(env);
			memcpy(envbuf.data(), wenv.c_str(), sizeof(wchar_t) * wenv.size());
		}

		PROCESS_INFORMATION pi = {};
		BOOL ok = CreateProcessW(nullptr,
								 wcmd.data(),
								 nullptr,
								 nullptr,
								 FALSE,
								 CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT,
								 envbuf.empty() ? nullptr : envbuf.data(),
								 nullptr,
								 &siEx.StartupInfo,
								 &pi);

		DeleteProcThreadAttributeList(siEx.lpAttributeList);
		HeapFree(GetProcessHeap(), 0, siEx.lpAttributeList);

		if (!use_input) {
			CloseHandle(hPipeInWrite_);
		}

		if (!ok) {
			CloseHandle(hPipeOutRead);
			ClosePseudoConsole(hPC);
			return {};
		}

		ResumeThread(pi.hThread);

		// プロセス終了後に ClosePseudoConsole することで hPipeOutRead が EOF になる
		WaitForSingleObject(pi.hProcess, INFINITE);
		GetExitCodeProcess(pi.hProcess, (DWORD *)&exit_code_);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		ClosePseudoConsole(hPC);

		char buf[256];
		DWORD n;
		while (ReadFile(hPipeOutRead, buf, sizeof(buf), &n, nullptr) && n > 0) {
			// ret.append(buf, n);
			writeOutput(buf, n);
		}
		CloseHandle(hPipeOutRead);

		output_vector_ = strip_vt(output_vector_);

		if (!output_vector_.empty() && output_vector_.back() == '\n') output_vector_.pop_back();
		if (!output_vector_.empty() && output_vector_.back() == '\r') output_vector_.pop_back();

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
// #include "win32/Win32PtyProcess.h"
constexpr static char const *conpty_agent_tag = "--conpty-agent---";
bool conpty_agent()
{
#if 0
	std::string cmd = GetCommandLineA();
	auto it = cmd.find(conpty_agent_tag);
	if (it != std::string::npos) {
		std::string tag = conpty_agent_tag;
		cmd = misc::trimmed(cmd.substr(it + tag.size()));
		cmd = base64_decode(cmd);
		exec_win_conpty(cmd, {});
		return true;
	}
#endif
	return false;
}
#endif

// experiment for process execution

void msleep(unsigned int ms)
{
#ifdef _WIN32
	Sleep(ms);
#else
	usleep(ms * 1000);
#endif
}

void process_test()
{
#if 0
	ProcessWinConPTY proc;
	proc.start("git --version", "");
	proc.wait();
	std::vector<char> out;
	proc.readResult(&out);
	std::string s(out.begin(), out.end());
	fprintf(stderr, "[%s]\n", s.c_str());
#elif 0
	ProcessWinConPTY proc;
	proc.start("sort", "");
	{
		// Windowsのsortコマンドは\r\n（CRLF）を行区切りとして期待する
		const char *input = "abc\r\ndef\r\n";
		proc.writeInput(input, strlen(input));
	}
	proc.wait();
	std::vector<char> out;
	proc.readResult(&out);
	std::string s(out.begin(), out.end());
	fprintf(stderr, "[%s]\n", s.c_str());
#elif 0
	ProcessPosixPty proc;
	proc.start("sort", "");
	{
		const char *input = "abc\ndef\n";
		proc.writeInput(input, strlen(input));
	}
	proc.wait();
	std::vector<char> out;
	proc.readResult(&out);
	std::string s(out.begin(), out.end());
	fprintf(stderr, "[%s]\n", s.c_str());
#elif 0
	ProcessPosix proc;
	proc.start("sort", "");
	{
		const char *input = "abc\ndef\n";
		proc.writeInput(input, strlen(input));
	}
	proc.wait();
	std::vector<char> out;
	proc.readResult(&out);
	std::string s(out.begin(), out.end());
	fprintf(stderr, "[%s]\n", s.c_str());
#endif
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




