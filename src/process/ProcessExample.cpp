
// experimental code for process execution, not used in production yet, may be removed without notice

#include "AbstractProcess.h"
#include "process/MyProcess2.h"
#include <stdio.h>
#include "main.h"
#include <string>
#include <thread>
#include <optional>
#include <QElapsedTimer>
#include "common/misc.h"
#include "Logger.h"

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
	std::vector<char> stdout_bytes_;
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
		thread_ = std::thread([&](const std::string &command, bool use_input){
			exec_win(command, use_input);
		}, command, use_input);
	}
	int wait()
	{
		if (thread_.joinable()) {
			thread_.join();
			GetExitCodeProcess(pi_.hProcess, (DWORD *)&exit_code_);
		}
		stdout_bytes_ = output_vector_;
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

	bool isRunning() const
	{
		return thread_.joinable();
	}
	const std::vector<char> &stdout_bytes() const
	{
		return stdout_bytes_;
	}
	const std::vector<char> &stderr_bytes() const
	{
		return {};
	}
};


class ProcessWinPty : public AbstractPtyProcess {
private:
	std::thread thread_;
	int input_fd_ = -1;
	HANDLE hConout_ = INVALID_HANDLE_VALUE;
	HANDLE hInput_ = INVALID_HANDLE_VALUE;
	int exit_code_ = 128;

	std::string exec_winpty(const std::string &cmd, const std::string &env, bool use_input)
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

		std::wstring program = convert_str_to_wstr(misc::getProgram(cmd));
		wchar_t const *program_p = nullptr;
		if (1) {
			// コマンドから実行ファイル名を抜き取る。実際に実行されるプログラムのパス。
			if (!program.empty()) {
				program_p = program.c_str();
			}
		} else {
			// nop:
			// program_p が nullptr 空の時、PATHが通っているコマンドなら実行できる。
		}

		std::wstring wenv = convert_str_to_wstr(env);
		std::vector<wchar_t> envbuf;
		if (!env.empty()) {
			envbuf.resize(wenv.size() + 1);
			memcpy(envbuf.data(), env.c_str(), sizeof(wchar_t) * env.size());
		}

		winpty_spawn_config_t *scfg = winpty_spawn_config_new(WINPTY_SPAWN_FLAG_AUTO_SHUTDOWN,
															  program_p,
															  wcmd.data(),
															  nullptr,
															  envbuf.empty() ? nullptr : envbuf.data(),
															  &err);
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
	void start(const std::string &cmd, std::string const &env, bool use_input)
	{
		thread_ = std::thread([&](std::string const &cmd, std::string const &env, bool use_input){
			exec_winpty(cmd, env, use_input);
		}, cmd, env, use_input);
	}
	bool wait(unsigned long time = ULONG_MAX)
	{
		closeInput();
		if (thread_.joinable()) {
			thread_.join();
			stdout_bytes_ = output_vector_;
			stderr_bytes_ = stderr_bytes_;
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

	int readOutputStreaming(char *ptr, int len) override
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
		BOOL ok = CreateProcessW(nullptr, wcmd.data(),
								 nullptr,
								 nullptr,
								 TRUE,
								 CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT,
								 nullptr,
								 nullptr,
								 &si,
								 &pi);

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
	void closeInput()
	{
		if (hPipeInWrite_ != INVALID_HANDLE_VALUE) {
			CloseHandle(hPipeInWrite_);
			hPipeInWrite_ = INVALID_HANDLE_VALUE;
		}
	}
	bool isRunning() const override { return false; }
	void writeInput(const char *ptr, int len) override
	{
		if (hPipeInWrite_ != INVALID_HANDLE_VALUE) {
			DWORD n;
			WriteFile(hPipeInWrite_, ptr, (DWORD)len, &n, nullptr);
		}
	}
	void start(const std::string &cmd, std::string const &env, bool use_input) override
	{
		thread_ = std::thread([&](std::string const &cmd, std::string const &env, bool use_input){
			exec_win_conpty(cmd, env, use_input);
		}, cmd, env, use_input);
	}
	bool wait(unsigned long time = ULONG_MAX) override
	{
		closeInput();
		if (thread_.joinable()) {
			thread_.join();
			stdout_bytes_ = output_vector_;
			stderr_bytes_ = {};
			return true;
		}
		return false;
	}
	void stop() override
	{
		wait();
	}
	int getExitCode() const override
	{
		return (int)exit_code_;
	}
	virtual int readOutputStreaming(char *ptr, int len)
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

#include "common/str.h"
#include "win32/Win32PtyProcess.h"
#include "ApplicationGlobal.h"

void process_test()
{
#ifdef _WIN32
#if 1
	std::string cmd = global->appsettings.git_command.toStdString();
	cmd = '"' + cmd + "\" --version";
#else
	std::string cmd = "git --version";
#endif
	ProcessWinPty proc;
	// Win32PtyProcess proc;
	// ProcessWinConPTY proc;
	proc.start(cmd, {}, false);
	// proc.stop();
#if 0
	while (!proc.wait(1)) {
		QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
	}
	proc.stop();
#else
	proc.wait();
#endif
	std::string s = (misc::str)proc.stdout_bytes();
	fprintf(stderr, "%s\n", s.c_str());
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




