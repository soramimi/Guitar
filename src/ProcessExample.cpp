
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

class ProcessPosix : public AbstractPtyProcess {
private:
	std::thread thread_;
	std::mutex mutex_;
	std::condition_variable cond_;
	int input_fd_ = -1;
	std::optional<ProcessResult> run(const std::string &cmd, const std::function<void (const char *, int)> &on_output = nullptr);
public:
	~ProcessPosix()
	{
		closeInput();
		if (thread_.joinable()) {
			thread_.join();
		}
	}
	bool isRunning() const override
	{
		return thread_.joinable();
	}
	void writeInput(const char *ptr, int len) override
	{
		::write(input_fd_, ptr, len);
	}
	void closeInput()
	{
		if (input_fd_ != -1) {
			close(input_fd_);
			input_fd_ = -1;
		}
	}
	int readOutput(char *ptr, int len) override
	{
		int n = output_queue_.size();
		if (n > len) n = len;
		for (int i = 0; i < n; i++) {
			ptr[i] = output_queue_.front();
			output_queue_.pop_front();
		}
		return n;
	}
	void start(const std::string &cmd, const std::string &env) override
	{
		thread_ = std::thread([&](){
			auto ret = run(cmd, [&](const char *ptr, int len){
				std::lock_guard<std::mutex> lock(mutex_);
				writeOutput(ptr, len);
			});
		});
		{
			std::unique_lock <std::mutex> lock(mutex_);
			cond_.wait(lock);
		}
	}
	bool wait(unsigned long time) override
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
		return 0;
	}
	void readResult(std::vector<char> *out) override
	{
		*out = output_vector_;
		output_vector_.clear();
	}
};

std::optional<ProcessResult> ProcessPosix::run(std::string const &cmd, std::function<void (const char *, int)> const &on_output)
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

	input_fd_ = pipe_stdin_fd[1];
	cond_.notify_all();

	ProcessResult ret;

	// closeInput();

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
				if (on_output) on_output(buf, (int)n);
			} else {
				close(stdout_rd);
				stdout_rd = -1;
			}
		}
		if (stderr_rd != -1 && FD_ISSET(stderr_rd, &fds)) {
			ssize_t n = read(stderr_rd, buf, sizeof(buf));
			if (n > 0) {
				if (on_output) on_output(buf, (int)n);
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

std::string exec_posixpty(std::string const &cmd)
{
	std::string ret;

	int master_fd;
	pid_t pid = forkpty(&master_fd, nullptr, nullptr, nullptr);
	if (pid == -1) {
		return ret;
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
	char buf[256];
	ssize_t n;
	while ((n = read(master_fd, buf, sizeof(buf))) > 0) {
		ret.append(buf, n);
	}
	close(master_fd);

	waitpid(pid, nullptr, 0);

	while (!ret.empty() && (ret.back() == '\n' || ret.back() == '\r')) {
		ret.pop_back();
	}

	return ret;
}

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

std::string exec_win(std::string const &cmd)
{
	std::string ret;

	HANDLE hReadPipe = nullptr;
	HANDLE hWritePipe = nullptr;
	SECURITY_ATTRIBUTES sa = {};
	sa.nLength = sizeof(sa);
	sa.bInheritHandle = TRUE;
	if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
		return ret;
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
	CloseHandle(hWritePipe);

	if (!ok) {
		CloseHandle(hReadPipe);
		return ret;
	}

	char buf[256];
	DWORD n;
	while (ReadFile(hReadPipe, buf, sizeof(buf), &n, nullptr) && n > 0) {
		ret.append(buf, n);
	}
	CloseHandle(hReadPipe);

	WaitForSingleObject(pi.hProcess, INFINITE);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	while (!ret.empty() && (ret.back() == '\n' || ret.back() == '\r')) {
		ret.pop_back();
	}

	return ret;
}

std::string exec_winpty(std::string const &cmd)
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

	HANDLE hProcess = nullptr;
	DWORD createError = 0;
	BOOL ok = winpty_spawn(wp, scfg, &hProcess, nullptr, &createError, &err);
	winpty_spawn_config_free(scfg);
	if (!ok) {
		winpty_error_free(err);
		winpty_free(wp);
		return ret;
	}

	HANDLE hConout = CreateFileW(
				winpty_conout_name(wp),
				GENERIC_READ, 0, nullptr,
				OPEN_EXISTING, 0, nullptr
				);

	if (hConout != INVALID_HANDLE_VALUE) {
		char buf[256];
		DWORD n;
		while (ReadFile(hConout, buf, sizeof(buf), &n, nullptr) && n > 0) {
			ret.append(buf, n);
		}
		CloseHandle(hConout);
	}

	WaitForSingleObject(hProcess, INFINITE);
	CloseHandle(hProcess);
	winpty_free(wp);

	while (!ret.empty() && (ret.back() == '\n' || ret.back() == '\r')) {
		ret.pop_back();
	}
	return ret;
}

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
public:
	bool isRunning() const override { return false; }
	void writeInput(const char *ptr, int len) override {}
	int readOutput(char *ptr, int len) override { return 0; }
	void start(const std::string &cmd, const std::string &env) override
	{
	}
	bool wait(unsigned long time) override
	{
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
		*out = std::move(out_);
	}
};

std::optional<std::vector<char>> exec_win_conpty(std::string const &cmd, std::string const &env)
{
	DWORD exit_code = 0;

	HANDLE hPipeInRead = nullptr, hPipeInWrite = nullptr;
	HANDLE hPipeOutRead = nullptr, hPipeOutWrite = nullptr;

	if (!CreatePipe(&hPipeInRead, &hPipeInWrite, nullptr, 0)) {
		return {};
	}
	if (!CreatePipe(&hPipeOutRead, &hPipeOutWrite, nullptr, 0)) {
		CloseHandle(hPipeInRead);
		CloseHandle(hPipeInWrite);
		return {};
	}

	HPCON hPC = nullptr;
	COORD size = {80, 25};
	HRESULT hr = CreatePseudoConsole(size, hPipeInRead, hPipeOutWrite, 0, &hPC);
	// ConPTY が両端を引き継ぐので呼び出し側のコピーを閉じる
	CloseHandle(hPipeInRead);
	CloseHandle(hPipeOutWrite);
	if (FAILED(hr)) {
		CloseHandle(hPipeInWrite);
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
		CloseHandle(hPipeInWrite);
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
	CloseHandle(hPipeInWrite);

	if (!ok) {
		CloseHandle(hPipeOutRead);
		ClosePseudoConsole(hPC);
		return {};
	}

	ResumeThread(pi.hThread);

	// プロセス終了後に ClosePseudoConsole することで hPipeOutRead が EOF になる
	WaitForSingleObject(pi.hProcess, INFINITE);
	GetExitCodeProcess(pi.hProcess, (DWORD *)&exit_code);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	ClosePseudoConsole(hPC);

	std::vector<char> out;
	char buf[256];
	DWORD n;
	while (ReadFile(hPipeOutRead, buf, sizeof(buf), &n, nullptr) && n > 0) {
		// ret.append(buf, n);
		out.insert(out.end(), buf, buf + n);
	}
	CloseHandle(hPipeOutRead);

	out = strip_vt(out);

	if (!out.empty() && out.back() == '\n') out.pop_back();
	if (!out.empty() && out.back() == '\r') out.pop_back();

	return out;
}

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
void process_test2()
{
	QElapsedTimer t;
	t.start();
#ifdef _WIN32
#if 1
	std::string cmd = GetCommandLineA();
	auto it = cmd.rfind('\\');
	if (it != std::string::npos) {
		cmd = cmd.substr(0, it);
		cmd += "\\conpty-agent.exe";
	}
	cmd += ' ';
	cmd += conpty_agent_tag;
	cmd += ' ';
	cmd += base64_encode("git --version");
	std::string s = exec_win(cmd);
#elif 0
	// std::string s = exec_win("git --version");
#elif 0
	// std::string s = exec_winpty("git --version");
#elif 1
	auto vec = exec_win_conpty("git --version", {});
	std::string s;
	if (vec) {
		s.assign(vec->begin(), vec->end());
	}
#endif
#else
#if 1
	// auto ret = exec_posix("git --version");
#elif 1
	std::string s = exec_posixpty("git --version");
#endif

#endif
	// fprintf(stderr, "[%s]\n", s.c_str());
	// fprintf(stderr, "%d ms\n", (int)t.elapsed());
}

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
	ProcessPosix proc;
	proc.start("base64", "");
	{
		const char *input = "Hello, world\n";
		proc.writeInput(input, strlen(input));
	}
	proc.wait(10000);
	std::vector<char> out;
	proc.readResult(&out);
	std::string s(out.begin(), out.end());
	fprintf(stderr, "[%s]\n", s.c_str());
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




