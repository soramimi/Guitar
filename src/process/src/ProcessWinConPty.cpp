#include "ProcessWinConPty.h"

struct ProcessWinConPty::Private {
	// state_mutex: started/running/exit_code などの軽い状態と、
	//   write_input/read_output などの短い操作を保護する。
	// op_mutex: conpty へのブロッキング呼び出し (wait/stop) を直列化する。
	//   ロック順序は常に op_mutex -> state_mutex (state_mutex は常に葉)。
	mutable std::mutex state_mutex;
	std::mutex op_mutex;
	BasicProcessWinConPTY conpty;
	bool started = false;
	bool running = false;
	int exit_code = -1;
};

ProcessWinConPty::ProcessWinConPty()
	: m(new Private())
{
	BasicProcessWinConPTY::Options opts;
	opts.output_vector = true;
	set_options(opts);
}

ProcessWinConPty::~ProcessWinConPty()
{
	stop();
	delete m;
}

void ProcessWinConPty::set_options(BasicProcessWinConPTY::Options const &options)
{
	m->conpty.set_options(options);
}

void ProcessWinConPty::start(std::string const &command, std::string const &env, bool use_input)
{
	(void)env;
	(void)use_input;
	std::unique_lock<std::mutex> op(m->op_mutex);
	std::lock_guard<std::mutex> lock(m->state_mutex);
	if (m->running) {
		m->conpty.wait();
		m->running = false;
	}
	m->exit_code = -1;
	if (command.empty()) {
		m->started = false;
		return;
	}
	if (!BasicProcessWinConPTY::is_conpty_available()) {
		m->started = false;
		m->running = false;
		return;
	}
	m->conpty.set_change_dir(change_dir_);
	m->started = m->conpty.start(command);
	m->running = m->started;
	if (m->started) {
		stdout_bytes_.clear();
		stderr_bytes_.clear();
	}
	m->exit_code = -1;
}

int ProcessWinConPty::wait()
{
	// conpty.wait() でブロックしている間は state_mutex を保持しない。
	// これにより待機中も write_input/read_output/stop が機能する。
	{
		std::lock_guard<std::mutex> lock(m->state_mutex);
		if (!m->running) {
			return m->exit_code;
		}
	}
	std::unique_lock<std::mutex> op(m->op_mutex);
	{
		// op_mutex 取得までの間に stop() が完了している可能性があるため再確認する
		std::lock_guard<std::mutex> lock(m->state_mutex);
		if (!m->running) {
			return m->exit_code;
		}
	}
	auto result = m->conpty.wait();
	std::lock_guard<std::mutex> lock(m->state_mutex);
	m->running = false;
	std::vector<char> const &out = m->conpty.stdout_bytes();
	stdout_bytes_.assign(out.begin(), out.end());
	stderr_bytes_.clear();
	m->exit_code = result.exit_code;
	return m->exit_code;
}

void ProcessWinConPty::stop()
{
	{
		std::lock_guard<std::mutex> lock(m->state_mutex);
		if (!m->running) {
			return;
		}
	}
	// 先に子プロセスを終了させる。別スレッドが wait() でブロックしていても、
	// これによりその待機が解除される (terminate() は内部で競合対策済み)。
	m->conpty.terminate();
	std::unique_lock<std::mutex> op(m->op_mutex);
	{
		std::lock_guard<std::mutex> lock(m->state_mutex);
		if (!m->running) {
			return;
		}
	}
	m->conpty.close_input();
	m->conpty.wait();
	std::lock_guard<std::mutex> lock(m->state_mutex);
	m->running = false;
	std::vector<char> const &out = m->conpty.stdout_bytes();
	stdout_bytes_.assign(out.begin(), out.end());
	stderr_bytes_.clear();
	m->exit_code = m->conpty.get_exit_code();
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
	if (m->running) {
		return m->conpty.read_output(ptr, len);
	}
	return 0;
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
