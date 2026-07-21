#include "ProcessWinConPtyWithWorker.h"
#include "BasicProcessWinConPty.h"
#include <algorithm>
#include <base64.h>
#include <misc.h>

//

#include <windows.h>
#include "ProcessWinHelper.h" // This file must be included after <windows.h>

struct ProcessWinConPtyWithWorker::Private {
	std::condition_variable cv;
	BasicProcessWin proc;
	bool started = false;
	bool running = false;
	bool waited = false;
	int exit_code = -1;
};

ProcessWinConPtyWithWorker::ProcessWinConPtyWithWorker()
	: m(new Private)
{
	BasicProcessWin::Options opts;
	opts.output_vector = true; // output monitoring
	set_options(opts);
}

ProcessWinConPtyWithWorker::~ProcessWinConPtyWithWorker()
{
	stop();
	delete m;
}

void ProcessWinConPtyWithWorker::set_options(BasicProcessWin::Options const &options)
{
	m->proc.set_options(options);
}

int ProcessWinConPtyWithWorker::run_worker(int argc, char **argv)
{
	bool as_worker = false;

	BasicProcessWinConPty::Options opts;
	std::string_view encoded;

	// ワーカー側のエラー (引数不正・デコード失敗・起動失敗) は 126 を返す。
	// 子プロセス自身の終了コードと区別できるようにするため (シェルの
	// 「コマンドを実行できない」慣習に合わせた)。128 は子が返しうる値なので使わない。
	int argi = 1;
	while (argi < argc) {
		std::string_view arg = argv[argi++];
		if (arg == subprocess_tag) {
			if (argi < argc) {
				encoded = argv[argi++];
				as_worker = true;
			} else {
				return 126;
			}
		} else if (arg == "--no-window") {
			opts.no_window = true;
		} else {
			return 126;
		}
	}

	if (as_worker && !encoded.empty()) {
		std::vector<char> decoded;
		if (!Base64::decode_checked(encoded.data(), encoded.size(), &decoded)) {
			return 126;
		}
		std::string cmd(decoded.data(), decoded.size());
		// 空コマンドや NUL 混入は不正な引数として拒否する
		if (cmd.empty() || cmd.find('\0') != std::string::npos) {
			return 126;
		}
		opts.output_stdout = true;
		BasicProcessWinConPty conpty(opts);
		conpty.start(cmd);
		auto result = conpty.wait();
		if (!result.started) {
			return 126;
		}
		return static_cast<int>(result.exit_code);
	}

	return 126; // not worker mode
}

void ProcessWinConPtyWithWorker::start(std::string const &command, std::string const &env, bool /*use_input*/)
{
	(void)env;
	stop();
	std::lock_guard<std::mutex> lock(mutex_);
	m->started = false;
	m->running = false;
	m->waited = false;
	m->exit_code = -1;
	if (command.empty()) {
		return;
	}
	if (!BasicProcessWinConPty::is_conpty_available()) {
		return;
	}

	std::wstring agent_path;
	{
		// Use dynamic buffer to avoid large stack array
		std::vector<wchar_t> tmp(32768);
		DWORD length = GetModuleFileNameW(nullptr, tmp.data(), static_cast<DWORD>(tmp.size()));
		if (length == 0 || length >= tmp.size()) {
			return;
		} else {
			std::wstring_view view(tmp.data(), length);
			auto i = view.find_last_of(L"/\\");
			agent_path = view.substr(0, i + 1);
			agent_path += L"conpty-worker.exe";
		}
	}
	DWORD attributes = GetFileAttributesW(agent_path.c_str());
	if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY)) {
		return;
	}

	std::string executable = convert_wstr_to_str(std::wstring_view(agent_path));
	if (executable.empty()) {
		return;
	}

	std::string cmd = misc::build_command_line({ executable, std::string(subprocess_tag), base64_encode(command) });

	m->proc.set_completion_callback([this](bool started, std::shared_ptr<void> user_data) {
		(void)started;
		(void)user_data;
		{
			std::lock_guard<std::mutex> lock(mutex_);
			m->proc.sync_output();
			this->output_vector_ = m->proc.stdout_bytes();
			this->stderr_bytes_.clear();
			m->running = false;
		}
		m->cv.notify_all();
		this->notify_completed();
	}, this->user_data_);

	m->proc.set_change_dir(change_dir_);
	m->started = m->proc.start(cmd);
	m->running = m->started;
	if (m->started) {
		output_vector_.clear();
		stderr_bytes_.clear();
	}
	m->exit_code = -1;
}



int ProcessWinConPtyWithWorker::_wait()
{
	auto result = m->proc.wait();

	std::lock_guard<std::mutex> lock(mutex_);
	m->proc.sync_output();
	std::vector<char> const &out = m->proc.stdout_bytes();
	if (!out.empty() || output_vector_.empty()) {
		output_vector_ = out;
	}
	stderr_bytes_.clear();
	if (result.started) {
		m->exit_code = result.exit_code;
	}
	m->running = false;
	m->waited = true;
	return m->exit_code;
}

bool ProcessWinConPtyWithWorker::wait(int time)
{
	{
		std::unique_lock<std::mutex> lock(mutex_);
		if (m->running) {
			bool b;
			if (time == INT_MAX) {
				m->cv.wait(lock, [this]() { return !m->running; });
				b = true;
			} else {
				b = m->cv.wait_for(lock, std::chrono::milliseconds(time), [this]() {
					return !m->running;
				});
			}
			if (!b) return false;
		}
		if (m->waited || !m->started) {
			return true;
		}
	}
	_wait();
	return true;
}

void ProcessWinConPtyWithWorker::stop()
{
	bool running = false;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		running = m->running;
	}
	if (running) {
		m->proc.close_input();
		m->proc.terminate();
	}
	if (running || m->started) {
		wait();
	}
}

bool ProcessWinConPtyWithWorker::is_running() const
{
	std::lock_guard<std::mutex> lock(mutex_);
	return m->running;
}

int ProcessWinConPtyWithWorker::get_exit_code() const
{
	std::lock_guard<std::mutex> lock(mutex_);
	return m->exit_code;
}

void ProcessWinConPtyWithWorker::write_input(char const *ptr, int len)
{
	if (!ptr || len <= 0) {
		return;
	}
	std::lock_guard<std::mutex> lock(mutex_);
	if (m->running) {
		m->proc.write_input(ptr, len);
	}
}

int ProcessWinConPtyWithWorker::read_output(char *ptr, int len)
{
	if (!ptr || len <= 0) {
		return 0;
	}
	int n = m->proc.read_output(ptr, len);
	return n;
}

void ProcessWinConPtyWithWorker::close_input()
{
	std::lock_guard<std::mutex> lock(mutex_);
	m->proc.close_input();
}

bool ProcessWinConPtyWithWorker::wait_for_output(std::string const &text)
{
	return m->proc.wait_for_output(text);
}
