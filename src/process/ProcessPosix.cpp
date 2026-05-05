#include "ProcessPosix.h"

#include <condition_variable>
#include <mutex>
#include <thread>

#include <unistd.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <pty.h>

// ProcessPosix

struct ProcessPosix::Private {
	std::deque<char> stdout_queue;
	std::deque<char> stderr_queue;
	std::vector<char> stdout_vector;
	std::vector<char> stderr_vector;
	std::vector<char> stdout_bytes;
	std::vector<char> stderr_bytes;
	std::thread thread;
	mutable std::mutex mutex;
	std::condition_variable cond;
	bool input_ready = false;
	int input_fd = -1;
	int exit_code = 128;
};

ProcessPosix::ProcessPosix()
	: m(new Private)
{

}

ProcessPosix::~ProcessPosix()
{
	closeInput(true);
	if (m->thread.joinable()) {
		m->thread.join();
	}
	delete m;
}


void ProcessPosix::writeStdOut(const char *buf, size_t len)
{
	m->stdout_queue.insert(m->stdout_queue.end(), buf, buf + len);
	m->stdout_vector.insert(m->stdout_vector.end(), buf, buf + len);
}

void ProcessPosix::writeStdErr(const char *buf, size_t len)
{
	m->stderr_queue.insert(m->stderr_queue.end(), buf, buf + len);
	m->stderr_vector.insert(m->stderr_vector.end(), buf, buf + len);
}

std::optional<process::ProcessResult> ProcessPosix::run(const std::string &cmd, bool use_input)
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
		misc::parse_args(cmd, &args);
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
		std::lock_guard<std::mutex> lock(m->mutex);
		m->input_fd = pipe_stdin_fd[1];
		m->input_ready = true;
	}
	m->cond.notify_all();

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
				std::lock_guard<std::mutex> lock(m->mutex);
				writeStdOut(buf, (size_t)n);
			} else {
				close(stdout_rd);
				stdout_rd = -1;
			}
		}
		if (stderr_rd != -1 && FD_ISSET(stderr_rd, &fds)) {
			ssize_t n = read(stderr_rd, buf, sizeof(buf));
			if (n > 0) {
				std::lock_guard<std::mutex> lock(m->mutex);
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

bool ProcessPosix::isRunning() const
{
	return m->thread.joinable();
}

void ProcessPosix::writeInput(const char *ptr, int len)
{
	::write(m->input_fd, ptr, len);
}

int ProcessPosix::readOutput(char *ptr, int len)
{
	int n = m->stdout_queue.size();
	if (n > len) n = len;
	for (int i = 0; i < n; i++) {
		ptr[i] = m->stdout_queue.front();
		m->stdout_queue.pop_front();
	}
	return n;
}

void ProcessPosix::start(const std::string &command, bool use_input)
{
	m->thread = std::thread([&](){
		auto ret = run(command, use_input);
		if (ret) {
			m->exit_code = ret->exit_code;
		} else {
			m->exit_code = 128;
		}
	});
	{
		std::unique_lock<std::mutex> lock(m->mutex);
		m->cond.wait(lock, [this]{ return m->input_ready; });
	}
}

int ProcessPosix::wait()
{
	(void)time;
	closeInput(true);
	if (m->thread.joinable()) {
		m->thread.join();
		return m->exit_code;
	}
	return 128;
}

void ProcessPosix::stop()
{
	wait();
}

int ProcessPosix::getExitCode() const
{
	return m->exit_code;
}

void ProcessPosix::readResult(std::vector<char> *out)
{
	std::lock_guard<std::mutex> lock(m->mutex);
	out->insert(out->end(), m->stdout_queue.begin(), m->stdout_queue.end());
	m->stdout_queue.clear();
}

std::vector<char> const &ProcessPosix::stdout_bytes() const
{
	std::lock_guard<std::mutex> lock(m->mutex);
	m->stderr_bytes = m->stderr_vector;
	return m->stderr_bytes;
}

std::vector<char> const &ProcessPosix::stderr_bytes() const
{
	std::lock_guard<std::mutex> lock(m->mutex);
	m->stdout_bytes = m->stdout_vector;
	return m->stdout_bytes;
}

void ProcessPosix::closeInput(bool justnow)
{
	if (m->input_fd != -1) {
		close(m->input_fd);
		m->input_fd = -1;
	}
}

// ProcessPosixPty

struct ProcessPosixPty::Private {
	std::deque<char> stdout_queue;
	std::deque<char> stderr_queue;
	std::vector<char> stdout_vector;
	std::vector<char> stderr_vector;
	std::thread thread;
	mutable std::mutex mutex;
	std::condition_variable cond;
	bool input_ready = false;
	int input_fd = -1;
	int exit_code = 128;

};


void ProcessPosixPty::writeStdOut(const char *buf, size_t len)
{
	m->stdout_queue.insert(m->stdout_queue.end(), buf, buf + len);
	m->stdout_vector.insert(m->stdout_vector.end(), buf, buf + len);
}

void ProcessPosixPty::writeStdErr(const char *buf, size_t len)
{
	m->stderr_queue.insert(m->stderr_queue.end(), buf, buf + len);
	m->stderr_vector.insert(m->stderr_vector.end(), buf, buf + len);
}

bool ProcessPosixPty::exec_posixpty(const std::string &cmd)
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
		misc::parse_args(cmd, &args);
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
		std::lock_guard<std::mutex> lock(m->mutex);
		m->input_fd = master_fd;
		m->input_ready = true;
	}
	m->cond.notify_all();

	char buf[256];
	ssize_t n;
	while ((n = read(master_fd, buf, sizeof(buf))) > 0) {
		std::lock_guard<std::mutex> lock(m->mutex);
		writeStdOut(buf, (size_t)n);
	}
	close(master_fd);
	{
		std::lock_guard<std::mutex> lock(m->mutex);
		m->input_fd = -1;
	}

	waitpid(pid, nullptr, 0);

	if (!m->stderr_vector.empty() && m->stderr_vector.back() == '\n') m->stderr_vector.pop_back();
	if (!m->stderr_vector.empty() && m->stderr_vector.back() == '\r') m->stderr_vector.pop_back();

	return true;
}

ProcessPosixPty::ProcessPosixPty()
	: m(new Private)
{
}

ProcessPosixPty::~ProcessPosixPty()
{
	stop();
	delete m;
}

bool ProcessPosixPty::isRunning() const
{
	return m->thread.joinable();
}

void ProcessPosixPty::writeInput(const char *ptr, int len)
{
	int fd;
	{
		std::lock_guard<std::mutex> lock(m->mutex);
		fd = m->input_fd;
	}
	if (fd != -1) {
		::write(fd, ptr, len);
	}
}

void ProcessPosixPty::closeInput()
{
	int fd;
	{
		std::lock_guard<std::mutex> lock(m->mutex);
		fd = m->input_fd;
	}
	if (fd != -1) {
		char eof_char = 0x04; // Ctrl+D: PTY canonical mode EOF
		::write(fd, &eof_char, 1);
	}
}

int ProcessPosixPty::readOutputStreaming(char *ptr, int len)
{
	std::lock_guard<std::mutex> lock(m->mutex);
	int n = m->stdout_queue.size();
	if (n > len) n = len;
	for (int i = 0; i < n; i++) {
		ptr[i] = m->stdout_queue.front();
		m->stdout_queue.pop_front();
	}
	return n;
}

void ProcessPosixPty::start(const std::string &cmd, const std::string &env, bool use_input)
{
	(void)use_input;
	m->thread = std::thread([this, cmd](){
		if (exec_posixpty(cmd)) {

		}
	});
	{
		std::unique_lock<std::mutex> lock(m->mutex);
		m->cond.wait(lock, [this]{ return m->input_ready; });
	}
}

bool ProcessPosixPty::wait(unsigned long time)
{
	(void)time;
	closeInput();
	if (m->thread.joinable()) {
		m->thread.join();
		return true;
	}
	return true;
}

void ProcessPosixPty::stop()
{
	wait();
}

int ProcessPosixPty::getExitCode() const
{
	return 0;
}

void ProcessPosixPty::readResult(std::vector<char> *out)
{
	std::lock_guard<std::mutex> lock(m->mutex);
	out->insert(out->end(), m->stdout_queue.begin(), m->stdout_queue.end());
	m->stdout_queue.clear();
}

std::string ProcessPosixPty::outstring() const
{
	std::string ret;
	{
		std::lock_guard<std::mutex> lock(m->mutex);
		ret.insert(ret.end(), m->stdout_vector.begin(), m->stdout_vector.end());
	}
	return ret;
}

std::string ProcessPosixPty::errstring() const
{
	std::string ret;
	{
		std::lock_guard<std::mutex> lock(m->mutex);
		ret.insert(ret.end(), m->stderr_vector.begin(), m->stderr_vector.end());
	}
	return ret;
}
