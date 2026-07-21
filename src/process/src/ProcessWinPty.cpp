#include "ProcessWinPty.h"
#include "ProcessHelper.h"
#include <windows.h>
#include <winpty.h>

#include "ProcessWinHelper.h" // This file must be included after <windows.h>

namespace {

class OutputReaderThread2 {
	friend class ::ProcessWinPty;

private:
	std::thread thread_;
	std::atomic<bool> interrupted_ { false };
	HANDLE handle_ = INVALID_HANDLE_VALUE;
	std::function<void(char const *, size_t)> sink_;

public:
	~OutputReaderThread2()
	{
		wait();
	}
	// 読み取ったデータは sink_ 経由で AbstractPtyProcess::write_output に渡す。
	// これにより出力バッファのロックが基底クラスの mutex_ に一元化される。
	void start(HANDLE hOutput, std::function<void(char const *, size_t)> sink)
	{
		handle_ = hOutput;
		sink_ = std::move(sink);
		interrupted_ = false;
		thread_ = std::thread([this]() {
			char buf[1024];
			while (1) {

				if (interrupted_) break;
				DWORD amount = 0;
				BOOL ret = ReadFile(handle_, buf, sizeof(buf), &amount, nullptr);
				if (!ret || amount == 0) {
					break;
				}
				if (sink_) {
					sink_(buf, amount);
				}
			}
		});
	}
	void wait()
	{
		if (thread_.joinable()) {
			thread_.join();
		}
	}
	void closeHandle()
	{
		if (handle_ != INVALID_HANDLE_VALUE) {
			CloseHandle(handle_);
			handle_ = INVALID_HANDLE_VALUE;
		}
	}
	void terminate()
	{
		interrupted_ = true;
		// ReadFile でブロックしているスレッドを解放するためハンドルを閉じる
		closeHandle();
		wait();
	}
	void interrupt()
	{
		interrupted_ = true;
		closeHandle();
	}
};

} // namespace

// ProcessWinPty

struct ProcessWinPty::Private {
	std::atomic<bool> interrupted { false };
	std::thread thread;
	std::mutex mutex;
	std::condition_variable cv;
	bool completed = true;
	std::string command;
	std::string env;
	std::string error_message;
	OutputReaderThread2 th_output_reader;
	AutoHandle hProcess;
	AutoHandle hOutput;
	AutoHandle hInput;
	DWORD exit_code = 0;
};

ProcessWinPty::ProcessWinPty()
	: m(new Private)
{
}

ProcessWinPty::~ProcessWinPty()
{
	// join されていない std::thread の破棄は std::terminate を呼ぶため、必ず止める
	stop();
	delete m;
}

bool ProcessWinPty::is_running() const
{
	return m->thread.joinable();
}

void ProcessWinPty::run()
{
	std::wstring program;
	wchar_t const *program_p = nullptr;
	if (1) {
		// コマンドから実行ファイル名を抜き取る。実際に実行されるプログラムのパス。
		program = convert_str_to_wstr(getProgram(m->command));
		if (!program.empty()) {
			program_p = program.c_str();
		}
	} else {
		// nop:
		// program_p が nullptr 空の時、PATHが通っているコマンドなら実行できる。
	}

	process::helper::PushDir chdir(change_dir_);

	winpty_config_t *agent_cfg = winpty_config_new(WINPTY_FLAG_PLAIN_OUTPUT, nullptr);
	if (!agent_cfg) {
		m->exit_code = 127;
		m->error_message = "winpty_config_new failed";
		notify_completed();
		return;
	}
	winpty_t *pty = winpty_open(agent_cfg, nullptr);
	winpty_config_free(agent_cfg);
	if (!pty) {
		m->exit_code = 127;
		m->error_message = "winpty_open failed";
		fprintf(stderr, "Failed to open winpty\n");
		notify_completed();
		return;
	}

	m->hInput = CreateFileW(winpty_conin_name(pty), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
	m->hOutput = CreateFileW(winpty_conout_name(pty), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
	if (!IS_VALID_HANDLE(m->hInput) || !IS_VALID_HANDLE(m->hOutput)) {
		m->exit_code = 127;
		m->error_message = "failed to open winpty conin/conout";
		winpty_free(pty);
		notify_completed();
		return;
	}
	m->th_output_reader.start(m->hOutput, [this](char const *buf, size_t len) {
		write_output(buf, len);
	});

	std::vector<wchar_t> envbuf;
#ifdef APP_GUITAR
	std::wstring env = convert_str_to_wstr(m->env);
	if (!env.empty()) {
		envbuf.resize(env.size() + 1);
		memcpy(envbuf.data(), env.c_str(), sizeof(wchar_t) * (env.size() + 1));
	}
#endif

	std::wstring wcmd = convert_str_to_wstr(m->command);

	winpty_spawn_config_t *spawn_cfg = winpty_spawn_config_new(WINPTY_SPAWN_FLAG_AUTO_SHUTDOWN,
		program_p,
		(wchar_t const *)wcmd.data(),
		change_dir_.empty() ? nullptr : change_dir_.c_str(),
		envbuf.empty() ? nullptr : envbuf.data(),
		nullptr);
	if (!spawn_cfg) {
		m->exit_code = 127;
		m->error_message = "winpty_spawn_config_new failed";
		m->th_output_reader.interrupt();
		close_input();
		winpty_free(pty);
		notify_completed();
		return;
	}
	BOOL spawnSuccess = winpty_spawn(pty, spawn_cfg, &m->hProcess, nullptr, nullptr, nullptr);
	winpty_spawn_config_free(spawn_cfg);
	if (!spawnSuccess) {
		m->exit_code = 127;
		m->error_message = "winpty_spawn failed";
		m->th_output_reader.interrupt();
		m->hOutput.close();
	}

	bool ok = false;

	if (spawnSuccess) {
		while (1) {
			if (m->interrupted) break;
			GetExitCodeProcess(m->hProcess, &m->exit_code);
			if (m->exit_code == STILL_ACTIVE) {
				std::unique_lock<std::mutex> lock(m->mutex);
				m->cv.wait_for(lock, std::chrono::milliseconds(1), [this] { return m->interrupted.load(); });
			} else {
				ok = true;
				(void)ok;
				break;
			}
		}
	}

	// プロセスの出力を確実に取得するため、ここで output reader スレッドの終了を待つ
	m->th_output_reader.wait();

	winpty_free(pty);

	close_input();
	m->hOutput.close();
	m->hProcess.close();

	notify_completed();
}

int ProcessWinPty::read_output(char *dstptr, int maxlen)
{
	return pop_output(dstptr, maxlen);
}

void ProcessWinPty::write_input(char const *ptr, int len)
{
	if (!ptr || len <= 0 || !IS_VALID_HANDLE(m->hInput)) {
		return;
	}
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
				write_all(m->hInput, left, static_cast<size_t>(right - left));
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
				char ch = static_cast<char>(c);
				write_all(m->hInput, &ch, 1);
			}
			left = right;
		} else {
			right++;
		}
	}
}

void ProcessWinPty::start(std::string const &cmdline, std::string const &env, bool use_input)
{
	(void)use_input;
	if (is_running()) return;
	m->command = cmdline;
	m->env = env;
	m->error_message.clear();
	m->interrupted = false;
	{
		std::lock_guard<std::mutex> lock(m->mutex);
		m->completed = false;
	}
	m->thread = std::thread([&]() {
		run();
		{
			std::lock_guard<std::mutex> lock(m->mutex);
			m->completed = true;
		}
		m->cv.notify_all();
	});
}

bool ProcessWinPty::wait(int time)
{
	if (m->thread.joinable()) {
		std::unique_lock<std::mutex> lock(m->mutex);
		bool done;
		if (time == INT_MAX) {
			m->cv.wait(lock, [this]() { return m->completed; });
			done = true;
		} else {
			done = m->cv.wait_for(lock, std::chrono::milliseconds(time), [this]() {
				return m->completed;
			});
		}
		if (!done) {
			return false;
		}
		lock.unlock();
		m->thread.join();
		stderr_bytes_ = { };
	}
	return true;
}

void ProcessWinPty::stop()
{
	// プロセススレッドに中断を通知してから、出力読み出しスレッドを止める
	// (ReadFile でブロックしていると winpty プロセスが終了してくれない)
	m->interrupted = true;
	m->th_output_reader.terminate();
	wait();
}

void ProcessWinPty::close_input()
{
	m->hInput.close();
}

int ProcessWinPty::get_exit_code() const
{
	return m->exit_code;
}

std::string const &ProcessWinPty::get_error_message() const
{
	return m->error_message;
}
