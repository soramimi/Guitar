#include "ProcessWinConPty.h"

struct ProcessWinConPty::Private {
	// state_mutex: started/running/exit_code などの軽い状態と、
	//   write_input/read_output などの短い操作を保護する。
	// op_mutex: conpty へのブロッキング呼び出し (wait/stop) を直列化する。
	//   ロック順序は常に op_mutex -> state_mutex (state_mutex は常に葉)。
	mutable std::mutex state_mutex;
	std::mutex op_mutex;
	BasicProcessWinConPty conpty;
	std::thread thread;
	std::condition_variable cv;
	bool running = false;
	int exit_code = -1;
	std::string error_message;
};

ProcessWinConPty::ProcessWinConPty()
	: m(new Private())
{
	BasicProcessWinConPty::Options opts;
	opts.output_vector = false;
	opts.output_queue = true;
	opts.vt_stripped = false;
	opts.forward_parent_stdin = false;
	set_options(opts);
}

ProcessWinConPty::~ProcessWinConPty()
{
	stop();
	if (m->thread.joinable()) {
		m->thread.join();
	}
	delete m;
}

void ProcessWinConPty::set_options(BasicProcessWinConPty::Options const &options)
{
	m->conpty.set_options(options);
}

void ProcessWinConPty::start(std::string const &command, std::string const &env, bool use_input)
{
	(void)use_input;
	std::unique_lock<std::mutex> op(m->op_mutex);
	{
		std::lock_guard<std::mutex> lock(m->state_mutex);
		if (m->running) {
			return;
		}
	}
	if (m->thread.joinable()) {
		m->thread.join();
	}
	if (command.empty()) {
		return;
	}
	reset_output();
	if (!BasicProcessWinConPty::is_conpty_available()) {
		{
			std::lock_guard<std::mutex> state_lock(m->state_mutex);
			m->error_message = "ConPTY is not available";
		}
		notify_completed();
		return;
	}
	{
		std::lock_guard<std::mutex> state_lock(m->state_mutex);
		m->exit_code = -1;
		m->error_message.clear();
		m->running = true;
	}
	{
		std::lock_guard<std::mutex> output_lock(mutex_);
		output_vector_.clear();
		stderr_bytes_.clear();
	}
	m->conpty.set_change_dir(change_dir_);
	m->conpty.set_environment(env);
	m->thread = std::thread([this](std::string const &command){
		{
			// start中のConPTYハンドル構築とresize/read/writeを直列化する。
			std::lock_guard<std::mutex> lock(m->state_mutex);
			m->conpty.start(command);
		}
		auto result = m->conpty.wait();
		{
			std::lock_guard<std::mutex> lock(m->state_mutex);
			m->running = false;
			m->exit_code = result.exit_code_;
			m->error_message = result.error_message_;
		}
		{
			std::lock_guard<std::mutex> output_lock(mutex_);
			std::vector<char> const &out = m->conpty.stdout_bytes();
			output_vector_.assign(out.begin(), out.end());
			stderr_bytes_.clear();
		}
		notify_completed();
		m->cv.notify_all();
	}, command);
}

ProcessResult ProcessWinConPty::wait(int time)
{
	ProcessResult result;
	{
		std::lock_guard<std::mutex> lock(m->state_mutex);
		result.started_ = m->thread.joinable() || m->running || m->exit_code != -1;
		result.running_ = m->running;
		if (m->exit_code != -1) {
			result.exit_code_ = static_cast<std::uint32_t>(m->exit_code);
		}
		result.error_message_ = m->error_message;
	}
	{
		std::unique_lock<std::mutex> op(m->op_mutex);
		if (m->thread.joinable()) {
			bool b;
			if (time == INT_MAX) {
				m->cv.wait(op, [this]() {
					std::lock_guard<std::mutex> lock(m->state_mutex);
					return !m->running;
				});
				b = true;
			} else {
				b = m->cv.wait_for(op, std::chrono::milliseconds(time), [this]() {
					std::lock_guard<std::mutex> lock(m->state_mutex);
					return !m->running;
				});
			}
			if (!b) return result;
		}
	}
	if (m->thread.joinable()) {
		m->thread.join();
	}
	std::lock_guard<std::mutex> lock(m->state_mutex);
	result.running_ = false;
	result.started_ = result.started_ || m->exit_code != -1;
	if (m->exit_code != -1) {
		result.exit_code_ = static_cast<std::uint32_t>(m->exit_code);
	}
	result.error_message_ = m->error_message;
	return result;
}

void ProcessWinConPty::stop()
{
	bool running = false;
	{
		std::lock_guard<std::mutex> lock(m->state_mutex);
		running = m->running;
		if (running) {
			// Basic側のstart完了後にstate_mutexが解放されるため、起動直後でも
			// 有効なプロセスハンドルに対して停止を要求できる。
			m->conpty.terminate();
		}
	}
	if (running) {
		// Basic側のwait()はワーカースレッドだけが所有する。ここから二重にwaitせず、
		// terminateで解除されたワーカーの完了通知を待つ。
		std::unique_lock<std::mutex> op(m->op_mutex);
		m->cv.wait(op, [this]() {
			std::lock_guard<std::mutex> lock(m->state_mutex);
			return !m->running;
		});
	}
	if (m->thread.joinable()) {
		m->thread.join();
	}
}

bool ProcessWinConPty::is_running() const
{
	std::lock_guard<std::mutex> lock(m->state_mutex);
	return m->running;
}

int ProcessWinConPty::get_exit_code() const
{
	std::lock_guard<std::mutex> lock(m->state_mutex);
	return m->exit_code;
}

void ProcessWinConPty::write_input(char const *ptr, int len)
{
	if (!ptr || len <= 0) {
		return;
	}
	std::lock_guard<std::mutex> lock(m->state_mutex);
	if (m->running) {
		m->conpty.write_input(ptr, len);
	}
}

int ProcessWinConPty::read_output(char *ptr, int len)
{
	if (!ptr || len <= 0) {
		return 0;
	}
	std::lock_guard<std::mutex> lock(m->state_mutex);
	return m->conpty.read_output(ptr, len);
}

void ProcessWinConPty::resize_terminal(int columns, int rows)
{
	std::lock_guard<std::mutex> lock(m->state_mutex);
	m->conpty.resize_terminal(columns, rows);
}

void ProcessWinConPty::set_no_window(bool no_window)
{
	m->conpty.set_no_window(no_window);
}

void ProcessWinConPty::close_input()
{
	std::lock_guard<std::mutex> lock(m->state_mutex);
	m->conpty.close_input();
}
