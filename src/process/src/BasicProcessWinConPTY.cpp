#include "BasicProcessWinConPTY.h"
#include <windows.h>
#include "ProcessWinHelper.h" // This file must be included after <windows.h>

struct BasicProcessWinConPTY::Private {
	struct D {
		BOOL running = FALSE;
		AutoProcessInformation pi;
		AutoPseudoConsole hPC;
		AutoHandle hPipeInRead;
		AutoHandle hPipeInWrite;
		AutoHandle hPipeOutRead;
		AutoHandle hPipeOutWrite;
		_AbstractBasicProcess::ExecResult result;
		std::deque<char> output_queue;
		std::vector<char> output_vector;
		bool output_closed = false;
	} d;
	mutable std::mutex output_mutex;
	BasicProcessWinConPTY::Options options;
	process::helper::dir_string_t change_dir;
	std::vector<char> output_bytes;
	std::atomic<bool> stop_input { false };
	std::thread input_writer;
	std::thread output_reader;
	std::mutex input_mutex;
	std::mutex snap_mutex;
	HANDLE hProcess_snap = nullptr;
	DWORD last_exit_code = static_cast<DWORD>(-1);
};

BasicProcessWinConPTY::BasicProcessWinConPTY(Options const &options)
	: m(new Private)
{
	set_options(options);
}

BasicProcessWinConPTY::~BasicProcessWinConPTY()
{
	terminate();
	wait();
	delete m;
}

void BasicProcessWinConPTY::set_change_dir(process::helper::dir_string_t const &dir)
{
	m->change_dir = dir;
}

void BasicProcessWinConPTY::set_options(Options const &options)
{
	m->options = options;
}

bool BasicProcessWinConPTY::start(std::string const &cmd)
{
	wait();
	if (cmd.empty()) return false;
	m->d = { };

	// ConPTYから見た入力用と出力用の2本の匿名パイプを用意する。
	if (!CreatePipe(&m->d.hPipeInRead, &m->d.hPipeInWrite, nullptr, 0)) {
		DWORD error_code = GetLastError();
		m->d.result.error_code = error_code;
		m->d.result.error_message = misc::get_error_message(error_code);
		return false;
	}
	if (!CreatePipe(&m->d.hPipeOutRead, &m->d.hPipeOutWrite, nullptr, 0)) {
		DWORD error_code = GetLastError();
		m->d = { };
		m->d.result.error_code = error_code;
		m->d.result.error_message = misc::get_error_message(error_code);
		return false;
	}

	COORD size = { 80, 25 };
	HRESULT hr = CreatePseudoConsole(size, m->d.hPipeInRead, m->d.hPipeOutWrite, 0, &m->d.hPC);
	// ConPTY に接続する子プロセスを作成するまで、パイプ端は保持する。
	if (FAILED(hr)) {
		DWORD error_code = static_cast<DWORD>(hr);
		m->d = { };
		m->d.result.error_code = error_code;
		m->d.result.error_message = misc::get_error_message(error_code);
		return false;
	}

	SIZE_T attrSize = 0;
	InitializeProcThreadAttributeList(nullptr, 1, 0, &attrSize);
	if (attrSize == 0) {
		DWORD error_code = GetLastError();
		m->d = { };
		m->d.result.error_code = error_code;
		m->d.result.error_message = misc::get_error_message(error_code);
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
		m->d.result.error_code = error_code;
		m->d.result.error_message = misc::get_error_message(error_code);
		return false;
	}

	std::wstring wcmd = convert_str_to_wstr(cmd);

	DWORD creation_flags = CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT;
	if (m->options.no_window) {
		creation_flags |= CREATE_NO_WINDOW;
	}

	m->d.running = CreateProcessW(nullptr,
		wcmd.data(),
		nullptr,
		nullptr,
		FALSE,
		creation_flags,
		nullptr,
		m->change_dir.empty() ? nullptr : m->change_dir.c_str(),
		&siEx.StartupInfo,
		&m->d.pi);
	if (!m->d.running) {
		DWORD error_code = GetLastError();
		m->d.result.error_code = error_code;
		m->d.result.error_message = misc::get_error_message(error_code);
	}

	DeleteProcThreadAttributeList(siEx.lpAttributeList);
	HeapFree(GetProcessHeap(), 0, siEx.lpAttributeList);

	// CreateProcessW が完了するまで、CreatePseudoConsole に渡したパイプ端を保持する。

	m->d.hPipeInRead.close();
	m->d.hPipeOutWrite.close();

	if (!m->d.running) {
		DWORD error_code = m->d.result.error_code;
		std::string error_message = std::move(m->d.result.error_message);
		m->d = { };
		m->d.result.error_code = error_code;
		m->d.result.error_message = std::move(error_message);
		return false;
	}
	m->d.result.started = true;

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
	m->input_writer = std::thread([this, hStdInput] {
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
		// fprintf(stderr, "before ReadFile\n");
		while (ReadFile(m->d.hPipeOutRead, buf, sizeof(buf), &n, nullptr) && n > 0) {
			// fprintf(stderr, "ReadFile: %d\n", (int)n);
			std::string_view view(buf, n);
			std::string text;
			if (m->options.vt_stripped) {
				text = vt_stripper.append(view);
				view = std::string_view(text.data(), text.size());
				// fprintf(stderr, "Stripped: %d %s\n", (int)text.size(), text.c_str());
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
		// fprintf(stderr, "after ReadFile\n");
		{
			std::lock_guard<std::mutex> lock(m->output_mutex);
			m->d.output_closed = true;
		}
	});

	if (ResumeThread(m->d.pi->hThread) == static_cast<DWORD>(-1)) {
		DWORD error_code = GetLastError();
		m->d.result.error_code = error_code;
		m->d.result.error_message = misc::get_error_message(error_code);
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
		m->d.hPC.close();
		if (m->output_reader.joinable()) {
			m->output_reader.join();
		}
		DWORD ec = m->d.result.error_code;
		std::string msg = std::move(m->d.result.error_message);
		m->d = { };
		m->d.result.error_code = ec;
		m->d.result.error_message = std::move(msg);
		return false;
	}

	return true;
}

BasicProcessWinConPTY::ExecResult BasicProcessWinConPTY::wait()
{
	if (IS_VALID_HANDLE(m->d.pi->hProcess) || IS_VALID_HANDLE(m->d.pi->hThread)) {

		// Git終了後にClosePseudoConsoleすることでhPipeOutReadがEOFになる。
		WaitForSingleObject(m->d.pi->hProcess, INFINITE);
		GetExitCodeProcess(m->d.pi->hProcess, &m->d.result.exit_code);
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
		m->d.hPC.close();

		if (m->output_reader.joinable()) {
			m->output_reader.join();
		}
		m->d.hPipeOutRead.close();
	}

	m->output_bytes = std::move(m->d.output_vector);

	auto ret = std::move(m->d.result);
	m->last_exit_code = ret.exit_code;
	m->d = { };

	return ret;
}

void BasicProcessWinConPTY::terminate()
{
	std::lock_guard<std::mutex> lock(m->snap_mutex);
	if (IS_VALID_HANDLE(m->hProcess_snap)) {
		TerminateProcess(m->hProcess_snap, 1);
	}
}

void BasicProcessWinConPTY::close_input()
{
	std::lock_guard<std::mutex> lock(m->input_mutex);
	if (IS_VALID_HANDLE(m->d.hPipeInWrite)) {
		m->d.hPipeInWrite.close();
	}
}

int BasicProcessWinConPTY::write_input(char const *ptr, int n)
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

int BasicProcessWinConPTY::read_output(char *ptr, int len)
{
	if (!ptr || len <= 0) {
		return 0;
	}
	std::lock_guard<std::mutex> lock(m->output_mutex);
	if (m->d.output_queue.empty()) return 0;

	int n = std::min(len, static_cast<int>(m->d.output_queue.size()));
	for (int i = 0; i < n; ++i) {
		ptr[i] = m->d.output_queue.front();
		m->d.output_queue.pop_front();
	}
	return n;
}

bool BasicProcessWinConPTY::is_running() const
{
	return IS_VALID_HANDLE(m->d.pi->hProcess) || IS_VALID_HANDLE(m->d.pi->hThread);
}

std::vector<char> const &BasicProcessWinConPTY::stdout_bytes() const
{
	return m->output_bytes;
}

void BasicProcessWinConPTY::set_no_window(bool no_window)
{
	m->options.no_window = no_window;
}

int BasicProcessWinConPTY::get_exit_code() const
{
	return static_cast<int>(m->last_exit_code);
}

bool BasicProcessWinConPTY::is_conpty_available()
{
	HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
	if (!hKernel32) return false;
	return GetProcAddress(hKernel32, "CreatePseudoConsole") != nullptr;
}
