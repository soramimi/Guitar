#include "BasicProcessWinConPty.h"
#include <algorithm>
#include <windows.h>
#include "ProcessWinHelper.h" // This file must be included after <windows.h>

struct BasicProcessWinConPty::Private {
	struct D {
		BOOL running = FALSE;
		AutoProcessInformation pi;
		AutoPseudoConsole hPC;
		AutoHandle hPipeInRead;
		AutoHandle hPipeInWrite;
		AutoHandle hPipeOutRead;
		AutoHandle hPipeOutWrite;
		// _AbstractBasicProcess::ExecResult result;
		ProcessResult result;
		std::deque<char> output_queue;
		std::vector<char> output_vector;
		bool output_closed = false;
	} d;
	mutable std::mutex output_mutex;
	BasicProcessWinConPty::Options options;
	process::helper::dir_string_t change_dir;
	std::string environment;
	std::vector<char> output_bytes;
	std::atomic<bool> stop_input { false };
	std::thread input_writer;
	std::thread output_reader;
	std::mutex input_mutex;
	std::mutex pseudo_console_mutex;
	std::mutex snap_mutex;
	std::deque<char> remaining_output_queue;
	HANDLE hProcess_snap = nullptr;
	DWORD last_exit_code = static_cast<DWORD>(-1);
};

BasicProcessWinConPty::BasicProcessWinConPty(Options const &options)
	: m(new Private)
{
	set_options(options);
}

BasicProcessWinConPty::~BasicProcessWinConPty()
{
	terminate();
	wait();
	delete m;
}

void BasicProcessWinConPty::set_change_dir(process::helper::dir_string_t const &dir)
{
	m->change_dir = dir;
}

void BasicProcessWinConPty::set_environment(std::string const &environment)
{
	m->environment = environment;
}

void BasicProcessWinConPty::set_options(Options const &options)
{
	m->options = options;
}

bool BasicProcessWinConPty::start(std::string const &cmd)
{
	wait();
	if (cmd.empty()) return false;
	m->d = { };
	{
		std::lock_guard<std::mutex> lock(m->output_mutex);
		m->remaining_output_queue.clear();
	}

	// ConPTYから見た入力用と出力用の2本の匿名パイプを用意する。
	if (!CreatePipe(&m->d.hPipeInRead, &m->d.hPipeInWrite, nullptr, 0)) {
		DWORD error_code = GetLastError();
		m->d.result.error_code_ = error_code;
		m->d.result.error_message_ = misc::get_error_message(error_code);
		return false;
	}
	if (!CreatePipe(&m->d.hPipeOutRead, &m->d.hPipeOutWrite, nullptr, 0)) {
		DWORD error_code = GetLastError();
		m->d = { };
		m->d.result.error_code_ = error_code;
		m->d.result.error_message_ = misc::get_error_message(error_code);
		return false;
	}

	COORD size = {
		static_cast<SHORT>(std::max(1, std::min(m->options.columns, 32767))),
		static_cast<SHORT>(std::max(1, std::min(m->options.rows, 32767)))
	};
	HRESULT hr = CreatePseudoConsole(size, m->d.hPipeInRead, m->d.hPipeOutWrite, 0, &m->d.hPC);
	// ConPTY に接続する子プロセスを作成するまで、パイプ端は保持する。
	if (FAILED(hr)) {
		DWORD error_code = static_cast<DWORD>(hr);
		m->d = { };
		m->d.result.error_code_ = error_code;
		m->d.result.error_message_ = misc::get_error_message(error_code);
		return false;
	}

	SIZE_T attrSize = 0;
	InitializeProcThreadAttributeList(nullptr, 1, 0, &attrSize);
	if (attrSize == 0) {
		DWORD error_code = GetLastError();
		m->d = { };
		m->d.result.error_code_ = error_code;
		m->d.result.error_message_ = misc::get_error_message(error_code);
		return false;
	}

	STARTUPINFOEXW siEx = { };
	siEx.StartupInfo.wShowWindow = SW_HIDE;
	siEx.StartupInfo.cb = sizeof(siEx);
	siEx.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), 0, attrSize);
	if (!siEx.lpAttributeList
		|| !InitializeProcThreadAttributeList(siEx.lpAttributeList, 1, 0, &attrSize)
		|| !UpdateProcThreadAttribute(siEx.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, m->d.hPC, sizeof(m->d.hPC), nullptr, nullptr)) {
		DWORD error_code = siEx.lpAttributeList ? GetLastError() : ERROR_NOT_ENOUGH_MEMORY;
		if (siEx.lpAttributeList) HeapFree(GetProcessHeap(), 0, siEx.lpAttributeList);
		m->d = { };
		m->d.result.error_code_ = error_code;
		m->d.result.error_message_ = misc::get_error_message(error_code);
		return false;
	}

	std::wstring wcmd = convert_str_to_wstr(cmd);
	std::vector<wchar_t> environment_block;
	if (!m->environment.empty()) {
		std::vector<std::wstring> entries;
		wchar_t *parent = GetEnvironmentStringsW();
		if (parent) {
			for (wchar_t const *entry = parent; *entry; entry += wcslen(entry) + 1) {
				entries.emplace_back(entry);
			}
			FreeEnvironmentStringsW(parent);
		}
		for (size_t offset = 0; offset < m->environment.size();) {
			size_t const end = m->environment.find('\0', offset);
			size_t const length = end == std::string::npos
								  ? m->environment.size() - offset : end - offset;
			std::wstring value = convert_str_to_wstr(
									 std::string_view(m->environment.data() + offset, length));
			size_t const separator = value.find(L'=');
			if (separator != std::wstring::npos && separator > 0) {
				std::wstring const name = value.substr(0, separator);
				auto it = std::find_if(entries.begin(), entries.end(), [&name](std::wstring const &entry){
						  return entry.size() > name.size()
						  && entry[name.size()] == L'='
						  && _wcsnicmp(entry.c_str(), name.c_str(), name.size()) == 0;
						  });
				if (it == entries.end()) entries.push_back(std::move(value));
				else *it = std::move(value);
			}
			if (end == std::string::npos) break;
			offset = end + 1;
		}
		std::sort(entries.begin(), entries.end(), [](std::wstring const &a, std::wstring const &b){
			return _wcsicmp(a.c_str(), b.c_str()) < 0;
		});
		for (std::wstring const &entry : entries) {
			environment_block.insert(environment_block.end(), entry.begin(), entry.end());
			environment_block.push_back(L'\0');
		}
		environment_block.push_back(L'\0');
	}

	DWORD creation_flags = CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT;
	if (m->options.no_window) {
		creation_flags |= CREATE_NO_WINDOW;
	}

	bool ok = CreateProcessW(nullptr,
							 wcmd.data(),
							 nullptr,
							 nullptr,
							 FALSE,
							 creation_flags,
							 environment_block.empty() ? nullptr : environment_block.data(),
							 m->change_dir.empty() ? nullptr : m->change_dir.c_str(),
							 &siEx.StartupInfo,
							 &m->d.pi);

	if (!ok) {
		DWORD error_code = GetLastError();
		m->d.result.error_code_ = error_code;
		m->d.result.error_message_ = misc::get_error_message(error_code);
	}

	DeleteProcThreadAttributeList(siEx.lpAttributeList);
	HeapFree(GetProcessHeap(), 0, siEx.lpAttributeList);

	// CreateProcessW が完了するまで、CreatePseudoConsole に渡したパイプ端を保持する。

	m->d.hPipeInRead.close();
	m->d.hPipeOutWrite.close();

	if (!ok) {
		DWORD error_code = m->d.result.error_code_;
		std::string error_message = std::move(m->d.result.error_message_);
		m->d = { };
		m->d.result.error_code_ = error_code;
		m->d.result.error_message_ = std::move(error_message);
		return false;
	}

	m->d.result.started_ = true;
	m->d.result.running_ = true; // 起動した＆実行中

	// terminate() が wait() と競合しても安全なように、プロセスハンドルを
	// スナップショットとして保持する (wait() がハンドルを閉じる直前にクリアする)。
	{
		std::lock_guard<std::mutex> lock(m->snap_mutex);
		m->hProcess_snap = m->d.pi->hProcess;
	}

	HANDLE hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
	HANDLE hStdInput = GetStdHandle(STD_INPUT_HANDLE);

	// ワーカーstdinは監督プロセスが作った匿名パイプである。
	// PeekNamedPipeでデータの有無を確認してから読むことでReadFileの永久待機を避け、
	// Git終了後にstop_input_でこのスレッドを確実に停止できるようにする。
	m->stop_input = false;
	if (m->options.forward_parent_stdin) m->input_writer = std::thread([this, hStdInput] {
		if (IS_VALID_HANDLE(m->d.hPipeInWrite) && IS_VALID_HANDLE(hStdInput)) {
			char buf[256];
			while (!m->stop_input) {
				DWORD available = 0;
				if (!PeekNamedPipe(hStdInput, nullptr, 0, nullptr, &available, nullptr)) {
					break;
				}
				if (available == 0) {
					Sleep(10);
					continue;
				}

				DWORD n = 0;
				DWORD size = available < sizeof(buf) ? available : sizeof(buf);
				if (!ReadFile(hStdInput, buf, size, &n, nullptr) || n == 0) {
					break;
				}
				{
					// close_input() / write_input() とハンドル競合しないように直列化する
					std::lock_guard<std::mutex> lock(m->input_mutex);
					if (!write_all(m->d.hPipeInWrite, buf, n)) {
						break;
					}
				}
			}
		}
	});

	// ConPTYの出力はプロセスの実行中から継続して排出する必要がある。
	// VtStripperはスレッド内に値で保持し、ReadFile間で解析状態を維持する。
	m->output_reader = std::thread([this, hStdOutput, vt_stripper = VtStripper { }]() mutable {
		char buf[256];
		DWORD n;
		while (ReadFile(m->d.hPipeOutRead, buf, sizeof(buf), &n, nullptr) && n > 0) {
			std::string_view view(buf, n);
			std::string text;
			if (m->options.vt_stripped) {
				text = vt_stripper.append(view);
				view = std::string_view(text.data(), text.size());
			}
			if (!view.empty()) {
				if (m->options.output_stdout) {
					WriteFile(hStdOutput, view.data(), static_cast<DWORD>(view.size()), &n, nullptr);
				}
				std::lock_guard<std::mutex> lock(m->output_mutex);
				if (m->options.output_vector) {
					m->d.output_vector.insert(m->d.output_vector.end(), view.begin(), view.end());
				}
				if (m->options.output_queue) {
					m->d.output_queue.insert(m->d.output_queue.end(), view.begin(), view.end());
				}
			}
		}
		{
			std::lock_guard<std::mutex> lock(m->output_mutex);
			m->d.output_closed = true;
		}
	});

	if (ResumeThread(m->d.pi->hThread) == static_cast<DWORD>(-1)) {
		DWORD error_code = GetLastError();
		m->d.result.error_code_ = error_code;
		m->d.result.error_message_ = misc::get_error_message(error_code);
		TerminateProcess(m->d.pi->hProcess, 128);
		{
			std::lock_guard<std::mutex> lock(m->snap_mutex);
			m->hProcess_snap = nullptr;
		}
		m->d.pi.close();
		m->stop_input = true;
		if (m->input_writer.joinable()) {
			m->input_writer.join();
		}
		close_input();
		{
			std::lock_guard<std::mutex> lock(m->pseudo_console_mutex);
			m->d.hPC.close();
		}
		if (m->output_reader.joinable()) {
			m->output_reader.join();
		}
		DWORD ec = m->d.result.error_code_;
		std::string msg = std::move(m->d.result.error_message_);
		m->d = { };
		m->d.result.error_code_ = ec;
		m->d.result.error_message_ = std::move(msg);
		return false;
	}

	return true;
}

ProcessResult BasicProcessWinConPty::wait()
{
	if (IS_VALID_HANDLE(m->d.pi->hProcess) || IS_VALID_HANDLE(m->d.pi->hThread)) {

		// Git終了後にClosePseudoConsoleすることでhPipeOutReadがEOFになる。
		WaitForSingleObject(m->d.pi->hProcess, INFINITE);
		DWORD exit_code = static_cast<DWORD>(-1);
		if (GetExitCodeProcess(m->d.pi->hProcess, &exit_code)) {
			m->d.result.exit_code_ = exit_code;
		}
		{
			std::lock_guard<std::mutex> lock(m->snap_mutex);
			m->hProcess_snap = nullptr;
		}
		m->d.pi.close();

		// 入力転送スレッドを先に止めてから、ConPTY入力の書き込み端を閉じる。
		// Git実行中にこの端を閉じると、ConPTYが入力終了として扱う場合がある。
		m->stop_input = true;
		if (m->input_writer.joinable()) {
			m->input_writer.join();
		}
		close_input();

		// ClosePseudoConsole が生成する最終出力も、読み取りスレッドで受け取る。
		{
			std::lock_guard<std::mutex> lock(m->pseudo_console_mutex);
			m->d.hPC.close();
		}

		if (m->output_reader.joinable()) {
			m->output_reader.join();
		}
		m->d.hPipeOutRead.close();
		{
			std::lock_guard<std::mutex> lock(m->output_mutex);
			m->remaining_output_queue.insert(m->remaining_output_queue.end(),
				m->d.output_queue.begin(), m->d.output_queue.end());
			m->d.output_queue.clear();
		}
	}

	std::lock_guard<std::mutex> output_lock(m->output_mutex);
	m->output_bytes = std::move(m->d.output_vector);

	ProcessResult ret = std::move(m->d.result);
	m->last_exit_code = ret.exit_code_;
	m->d = { };

	ret.running_ = false;
	return ret;
}

void BasicProcessWinConPty::terminate()
{
	std::lock_guard<std::mutex> lock(m->snap_mutex);
	if (IS_VALID_HANDLE(m->hProcess_snap)) {
		TerminateProcess(m->hProcess_snap, 1);
	}
}

void BasicProcessWinConPty::close_input()
{
	std::lock_guard<std::mutex> lock(m->input_mutex);
	if (IS_VALID_HANDLE(m->d.hPipeInWrite)) {
		m->d.hPipeInWrite.close();
	}
}

int BasicProcessWinConPty::write_input(char const *ptr, int n)
{
	if (!ptr || n <= 0) {
		return 0;
	}
	std::lock_guard<std::mutex> lock(m->input_mutex);
	if (IS_VALID_HANDLE(m->d.hPipeInWrite)) {
		if (write_all(m->d.hPipeInWrite, ptr, static_cast<size_t>(n))) {
			return n;
		}
		return 0;
	}
	return -1;
}

int BasicProcessWinConPty::read_output(char *ptr, int len)
{
	if (!ptr || len <= 0) {
		return 0;
	}
	std::lock_guard<std::mutex> lock(m->output_mutex);
	std::deque<char> *queue = !m->d.output_queue.empty()
		? &m->d.output_queue : &m->remaining_output_queue;
	if (queue->empty()) return 0;

	int n = std::min(len, static_cast<int>(queue->size()));
	for (int i = 0; i < n; ++i) {
		ptr[i] = queue->front();
		queue->pop_front();
	}
	return n;
}

void BasicProcessWinConPty::resize_terminal(int columns, int rows)
{
	columns = std::max(1, std::min(columns, 32767));
	rows = std::max(1, std::min(rows, 32767));
	m->options.columns = columns;
	m->options.rows = rows;
	std::lock_guard<std::mutex> lock(m->pseudo_console_mutex);
	if (m->d.hPC) {
		COORD size = { static_cast<SHORT>(columns), static_cast<SHORT>(rows) };
		ResizePseudoConsole(m->d.hPC, size);
	}
}

bool BasicProcessWinConPty::is_running() const
{
	return IS_VALID_HANDLE(m->d.pi->hProcess) || IS_VALID_HANDLE(m->d.pi->hThread);
}

std::vector<char> const &BasicProcessWinConPty::stdout_bytes() const
{
	return m->output_bytes;
}

void BasicProcessWinConPty::set_no_window(bool no_window)
{
	m->options.no_window = no_window;
}

int BasicProcessWinConPty::get_exit_code() const
{
	return static_cast<int>(m->last_exit_code);
}

bool BasicProcessWinConPty::is_conpty_available()
{
	HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
	if (!hKernel32) return false;
	return GetProcAddress(hKernel32, "CreatePseudoConsole") != nullptr;
}
