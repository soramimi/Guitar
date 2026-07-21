
#include "ProcessWin.h"
#include <atomic>
#include <windows.h>
#include "ProcessWinHelper.h" // This file must be included after <windows.h>

namespace {

class OutputReaderThread {
private:
	HANDLE hRead_;
	std::thread thread_;
	std::mutex *mutex_;
	std::deque<char> *buffer_;

public:
	OutputReaderThread(HANDLE hRead, std::mutex *mutex, std::deque<char> *buffer)
		: hRead_(hRead)
		, mutex_(mutex)
		, buffer_(buffer)
	{
	}
	~OutputReaderThread()
	{
		stop();
	}
	void start()
	{
		thread_ = std::thread([this]() {
			char buf[4096];
			while (1) {
				DWORD len = 0;
				if (!ReadFile(hRead_, buf, sizeof(buf), &len, nullptr)) break;
				if (len < 1) break;
				if (buffer_) {
					std::lock_guard lock(*mutex_);
					buffer_->insert(buffer_->end(), buf, buf + len);
				}
			}
		});
	}
	void stop()
	{
		if (thread_.joinable()) {
			thread_.join();
		}
	}
	void wait()
	{
		stop();
	}
};

class ProcessWinThread {
public:
	std::thread thread_;
	std::mutex finished_mutex_;
	std::condition_variable finished_cv_;
	bool finished_ = true;
	std::mutex *mutex_ = nullptr;
	std::string command_;
	DWORD exit_code_ = -1;
	DWORD error_code_ = ERROR_SUCCESS;
	std::string error_message_;
	std::vector<char> input_;
	std::deque<char> outq_;
	std::deque<char> errq_;
	bool use_input_ = false;
	AutoHandle hInputWrite_;
	std::atomic<bool> close_input_later_ { false };
	std::mutex snap_mutex_;
	HANDLE hProcess_snap_ = nullptr;

	// 環境変数をキャッシュして再利用 (初回起動時の環境を使い続けることに注意)
	static std::vector<wchar_t> cached_env_;
	static std::mutex env_mutex_;

	void reset()
	{
		mutex_ = nullptr;
		command_.clear();
		exit_code_ = -1;
		error_code_ = ERROR_SUCCESS;
		error_message_.clear();
		input_.clear();
		outq_.clear();
		errq_.clear();
		use_input_ = false;
		hInputWrite_.close();
		close_input_later_ = false;
		finished_ = true;
	}

public:
	ProcessWinThread()
	{
	}
	~ProcessWinThread()
	{
		terminate();
		stop();
	}
	// mutex_ 配下で呼ぶ (スレッドの書き込みループが mutex_ を保持しているため)
	void closeInputLocked()
	{
		hInputWrite_.close();
	}
	void closeInput()
	{
		if (mutex_) {
			std::lock_guard<std::mutex> lock(*mutex_);
			hInputWrite_.close();
		} else {
			hInputWrite_.close();
		}
	}
	void terminate()
	{
		std::lock_guard<std::mutex> lock(snap_mutex_);
		if (IS_VALID_HANDLE(hProcess_snap_)) {
			TerminateProcess(hProcess_snap_, 1);
		}
	}
	void writeInput(char const *ptr, int len)
	{
		if (!ptr || len <= 0 || !mutex_) return;
		std::lock_guard lock(*mutex_);
		input_.insert(input_.end(), ptr, ptr + len);
	}
	void start()
	{
		{
			std::lock_guard<std::mutex> lock(finished_mutex_);
			finished_ = false;
		}
		thread_ = std::thread([this]() {
			auto run_impl = [this]() {
			hInputWrite_.close();
			error_code_ = ERROR_SUCCESS;
			error_message_.clear();

			AutoHandle hOutputRead;
			AutoHandle hOutputWrite;
			AutoHandle hInputRead;
			AutoHandle hInputWrite;
			AutoHandle hErrorRead;
			AutoHandle hErrorWrite;
			AutoProcessInformation pi;

			SECURITY_ATTRIBUTES sa;
			sa.nLength = sizeof(SECURITY_ATTRIBUTES);
			sa.lpSecurityDescriptor = nullptr;
			sa.bInheritHandle = TRUE;

			std::vector<wchar_t> *env_ptr = nullptr;

			auto fail = [this](char const *what) {
				DWORD ec = GetLastError();
				error_code_ = ec;
				error_message_ = std::string(what) + ": " + misc::get_error_message(ec);
			};

			std::wstring wcmd(convert_str_to_wstr(command_));
			if (wcmd.empty()) {
				error_message_ = "empty command";
				return;
			}

			// パイプ作成の最適化
			constexpr static DWORD PIPE_BUFFER_SIZE = 65536;

			if (!CreatePipe(&hInputRead, &hInputWrite, &sa, PIPE_BUFFER_SIZE)) {
				fail("CreatePipe (stdin)");
				return;
			}

			if (!CreatePipe(&hOutputRead, &hOutputWrite, &sa, PIPE_BUFFER_SIZE)) {
				fail("CreatePipe (stdout)");
				return;
			}

			if (!CreatePipe(&hErrorRead, &hErrorWrite, &sa, PIPE_BUFFER_SIZE)) {
				fail("CreatePipe (stderr)");
				return;
			}

			// ハンドルの継承可能性を最適化
			if (!SetHandleInformation(hInputWrite, HANDLE_FLAG_INHERIT, 0)
				|| !SetHandleInformation(hOutputRead, HANDLE_FLAG_INHERIT, 0)
				|| !SetHandleInformation(hErrorRead, HANDLE_FLAG_INHERIT, 0)) {
				fail("SetHandleInformation");
				return;
			}

			// プロセス起動
			STARTUPINFOW si;

			ZeroMemory(&si, sizeof(STARTUPINFOW));
			si.cb = sizeof(STARTUPINFOW);
			si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
			si.wShowWindow = SW_HIDE;
			si.hStdInput = hInputRead;
			si.hStdOutput = hOutputWrite;
			si.hStdError = hErrorWrite;

			{
				std::lock_guard<std::mutex> lock(env_mutex_);
				if (cached_env_.empty()) {
					wchar_t *p = GetEnvironmentStringsW();
					if (p) {
						int i = 0;
						while (p[i] || p[i + 1]) {
							i++;
						}
						cached_env_.assign(p, p + i + 1);
						FreeEnvironmentStringsW(p);

						// LANG=en_US.UTF8を追加 (既にLANGがあれば追加しない)
						bool has_lang = false;
						for (size_t j = 0; j < cached_env_.size() && cached_env_[j];) {
							wchar_t const *entry = &cached_env_[j];
							size_t len = wcslen(entry);
							if (wcsncmp(entry, L"LANG=", 5) == 0) {
								has_lang = true;
								break;
							}
							j += len + 1;
						}
						if (!has_lang) {
							wchar_t const *e = L"LANG=en_US.UTF8";
							cached_env_.insert(cached_env_.end() - 1, e, e + wcslen(e) + 1);
						}
					}
				}
				env_ptr = cached_env_.empty() ? nullptr : &cached_env_;
			}

			// CreateProcessの最適化フラグ
			DWORD creation_flags = CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT;

			if (!CreateProcessW(nullptr, wcmd.data(), nullptr, nullptr, TRUE, creation_flags, env_ptr ? env_ptr->data() : nullptr, nullptr, &si, &pi)) {
				fail("CreateProcessW");
				return;
			}

			// 子プロセス側のハンドルをすぐに閉じる
			hInputRead.close();
			hOutputWrite.close();
			hErrorWrite.close();

			// terminate() がハンドルクローズと競合しないようスナップショットを保持する
			{
				std::lock_guard<std::mutex> lock(snap_mutex_);
				hProcess_snap_ = pi->hProcess;
			}

			// 書き込みハンドルをメンバに移し、実行中の closeInput() が機能するようにする
			hInputWrite_ = hInputWrite.detach();

			if (!use_input_) {
				closeInput();
			}

			{
				OutputReaderThread t1(hOutputRead, mutex_, &outq_);
				OutputReaderThread t2(hErrorRead, mutex_, &errq_);
				t1.start();
				t2.start();

				HANDLE handles[] = { pi->hProcess };
				while (1) {
					DWORD wait_result = WaitForMultipleObjects(1, handles, FALSE, 10);
					if (wait_result == WAIT_OBJECT_0) break;
					if (wait_result == WAIT_FAILED) break;

					{
						std::lock_guard lock(*mutex_);
						if (!input_.empty()) {
							if (IS_VALID_HANDLE(hInputWrite_)) {
								if (!write_all(hInputWrite_, input_.data(), input_.size())) {
									closeInputLocked();
								}
								input_.clear();
							}
						} else if (close_input_later_) {
							closeInputLocked();
						}
					}
				}

				t1.wait();
				t2.wait();

				hOutputRead.close();
				hErrorRead.close();

				GetExitCodeProcess(pi->hProcess, &exit_code_);
				{
					std::lock_guard<std::mutex> lock(snap_mutex_);
					hProcess_snap_ = nullptr;
				}
				pi.close();
			}
			};
			run_impl();
			{
				std::lock_guard<std::mutex> lock(finished_mutex_);
				finished_ = true;
			}
			finished_cv_.notify_all();
		});
	}
	bool waitFor(int time)
	{
		if (!thread_.joinable()) {
			return true;
		}
		std::unique_lock<std::mutex> lock(finished_mutex_);
		if (time == INT_MAX) {
			finished_cv_.wait(lock, [this]() { return finished_; });
			return true;
		}
		return finished_cv_.wait_for(lock, std::chrono::milliseconds(time), [this]() { return finished_; });
	}
	void stop()
	{
		if (thread_.joinable()) {
			thread_.join();
		}
	}
	void wait()
	{
		stop();
	}
};

// 静的メンバーの定義
std::vector<wchar_t> ProcessWinThread::cached_env_;
std::mutex ProcessWinThread::env_mutex_;

std::string toQString(std::vector<char> const &vec)
{
	if (vec.empty()) return { };
	return std::string(&vec[0], vec.size());
}

} // namespace

struct ProcessWin::Private {
	std::mutex mutex;
	ProcessWinThread th;
	std::vector<char> stdout_bytes;
	std::vector<char> stderr_bytes;
	int exit_code = -1;
	int error_code = 0;
	std::string error_message;
};

ProcessWin::ProcessWin()
	: m(new Private)
{
}

ProcessWin::~ProcessWin()
{
	delete m;
}

void ProcessWin::start(std::string const &command, bool use_input)
{
	if (is_running()) {
		wait();
	}
	m->exit_code = -1;
	m->error_code = 0;
	m->error_message.clear();
	if (command.empty()) {
		return;
	}
	m->th.mutex_ = &m->mutex;
	m->th.use_input_ = use_input;
	m->th.command_ = command;
	m->th.start();
}

ProcessResult ProcessWin::wait(int time)
{
	ProcessResult result;
	result.started_ = m->th.thread_.joinable();
	result.running_ = result.started_;
	if (!m->th.waitFor(time)) {
		return result;
	}
	m->th.wait();

	m->stdout_bytes.clear();
	m->stderr_bytes.clear();
	m->stdout_bytes.insert(m->stdout_bytes.end(), m->th.outq_.begin(), m->th.outq_.end());
	m->stderr_bytes.insert(m->stderr_bytes.end(), m->th.errq_.begin(), m->th.errq_.end());
	m->exit_code = m->th.exit_code_;
	m->error_code = static_cast<int>(m->th.error_code_);
	m->error_message = std::move(m->th.error_message_);
	result.running_ = false;
	result.exit_code_ = static_cast<std::uint32_t>(m->exit_code);
	result.error_code_ = static_cast<std::uint32_t>(m->error_code);
	result.error_message_ = m->error_message;
	m->th.reset();
	return result;
}

bool ProcessWin::is_running() const
{
	return m->th.thread_.joinable();
}

void ProcessWin::write_input(char const *ptr, int len)
{
	m->th.writeInput(ptr, len);
}

void ProcessWin::close_input()
{
	_close_input(true);
}

void ProcessWin::_close_input(bool justnow)
{
	if (justnow) {
		m->th.closeInput();
	} else {
		m->th.close_input_later_ = true;
	}
}

void ProcessWin::stop()
{
	m->th.terminate();
	wait();
}

int ProcessWin::get_exit_code() const
{
	return m->exit_code;
}

int ProcessWin::get_error_code() const
{
	return m->error_code;
}

std::string const &ProcessWin::get_error_message() const
{
	return m->error_message;
}

std::vector<char> const &ProcessWin::stdout_bytes() const
{
	return m->stdout_bytes;
}

std::vector<char> const &ProcessWin::stderr_bytes() const
{
	return m->stderr_bytes;
}
