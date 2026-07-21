#include "BasicProcessPosix.h"
#include "ProcessHelper.h"
#include <cstring>
#include <deque>
#include <mutex>

#ifdef _WIN32
#include <io.h>
#else
#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <fcntl.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <thread>
#include <unistd.h>

#ifdef APP_GUITAR
#include <QDir>
#endif

#endif

class OutputReaderThread {
private:
	int fd;
	std::thread thread_;
	std::mutex *mutex_;
	std::deque<char> *buffer_;

protected:
	void run()
	{
		while (1) {
			char buf[1024];
			int n = read(fd, buf, sizeof(buf));
			if (n < 1) break;
			if (buffer_) {
				std::lock_guard<std::mutex> lock(*mutex_);
				buffer_->insert(buffer_->end(), buf, buf + n);
			}
		}
	}

public:
	OutputReaderThread(int fd, std::mutex *mutex, std::deque<char> *out)
		: fd(fd)
		, mutex_(mutex)
		, buffer_(out)
	{
	}
	~OutputReaderThread()
	{
		stop();
	}
	void start()
	{
		stop();
		thread_ = std::thread([this]() {
			run();
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

class ProcessPosixThread {
public:
	std::thread thread;
	std::mutex finished_mutex;
	std::condition_variable finished_cv;
	bool finished = true;
	std::mutex *mutex = nullptr;
	std::vector<std::string> argvec;
	std::vector<char *> args;
	std::deque<char> inq;
	std::deque<char> outq;
	std::deque<char> errq;
	bool use_input = false;
	int fd_in_read = -1;
	std::atomic<pid_t> pid { 0 };
	std::atomic<long long> term_deadline_ms { 0 };
	int exit_code = -1;
	int error_code = 0;
	std::string error_message;
	bool close_input_later = false;

protected:
public:
	void init(std::mutex *mutex, bool use_input)
	{
		this->mutex = mutex;
		this->use_input = use_input;
	}
	void reset()
	{
		argvec.clear();
		args.clear();
		inq.clear();
		outq.clear();
		errq.clear();
		use_input = false;
		fd_in_read = -1;
		pid = 0;
		term_deadline_ms = 0;
		exit_code = -1;
		close_input_later = false;
		finished = true;
	}

protected:
	void run()
	{
		exit_code = -1;
		int const R = 0;
		int const W = 1;
		int const E = 2;
		int stdin_pipe[3] = { -1, -1, -1 };
		int stdout_pipe[3] = { -1, -1, -1 };
		int stderr_pipe[3] = { -1, -1, -1 };
		int fd_out_write;
		int fd_err_write;
		pid_t child_pid;

		// 子プロセスがstdinを閉じた後にwriteするとSIGPIPEが発生する。
		// デフォルト動作ではホストプロセスごと終了してしまうため、このスレッドでは
		// SIGPIPEをブロックし、writeのEPIPEエラーとしてハンドリングする。
		{
			sigset_t set;
			sigemptyset(&set);
			sigaddset(&set, SIGPIPE);
			pthread_sigmask(SIG_BLOCK, &set, nullptr);
		}

		if (pipe(stdin_pipe) < 0) {
			error_code = errno;
			error_message = "failed: pipe (stdin)";
			goto fail;
		}

		if (pipe(stdout_pipe) < 0) {
			error_code = errno;
			error_message = "failed: pipe (stdout)";
			goto fail;
		}

		if (pipe(stderr_pipe) < 0) {
			error_code = errno;
			error_message = "failed: pipe (stderr)";
			goto fail;
		}

		child_pid = fork();
		if (child_pid < 0) {
			error_code = errno;
			error_message = "failed: fork";
			goto fail;
		}

		if (child_pid == 0) { // child
			setenv("LANG", "C", 1);
			close(stdin_pipe[W]);
			close(stdout_pipe[R]);
			close(stderr_pipe[R]);
			dup2(stdin_pipe[R], R);
			dup2(stdout_pipe[W], W);
			dup2(stderr_pipe[W], E);
			close(stdin_pipe[R]);
			close(stdout_pipe[W]);
			close(stderr_pipe[E]);
			if (execvp(args[0], &args[0]) < 0) {
				close(stdin_pipe[R]);
				close(stdout_pipe[W]);
				close(stderr_pipe[E]);
				// forkした子プロセス側なので、exit()（atexitハンドラやCライブラリの
				// バッファを親と共有した状態でフラッシュしてしまう）ではなく_exit()を使う。
				// またマルチスレッドの親からforkした直後はロック取得を伴うstdioが
				// デッドロックしうるため、async-signal-safeなwrite(2)だけを使う。
				char const msg[] = "failed: exec\n";
				if (write(STDERR_FILENO, msg, sizeof(msg) - 1) < 0) {
					// 書けなくても終了コード127で十分失敗は伝わる
				}
				_exit(127);
			}
		}
		pid = child_pid;

		close(stdin_pipe[R]);
		close(stdout_pipe[W]);
		close(stderr_pipe[W]);
		fd_in_read = stdin_pipe[W];
		fd_out_write = stdout_pipe[R];
		fd_err_write = stderr_pipe[R];

		//

		if (!use_input) {
			closeInput();
		}

		{
			OutputReaderThread t1(fd_out_write, mutex, &outq);
			OutputReaderThread t2(fd_err_write, mutex, &errq);
			t1.start();
			t2.start();

			while (1) {
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				int status = 0;
				pid_t wait_result = waitpid(child_pid, &status, WNOHANG);
				if (wait_result == child_pid) {
					if (WIFEXITED(status)) {
						exit_code = WEXITSTATUS(status);
						break;
					}
					if (WIFSIGNALED(status)) {
						exit_code = 128 + WTERMSIG(status);
						break;
					}
				} else if (wait_result < 0 && errno != EINTR) {
					break;
				}
				{
					// SIGTERM を無視する子のための SIGKILL エスカレーション
					long long dl = term_deadline_ms.load();
					if (dl != 0) {
						long long now = std::chrono::duration_cast<std::chrono::milliseconds>(
							std::chrono::steady_clock::now().time_since_epoch())
											.count();
						if (now >= dl) {
							kill(child_pid, SIGKILL);
							term_deadline_ms = 0;
						}
					}
				}
				{
					std::lock_guard<std::mutex> lock(*mutex);
					int n = inq.size();
					if (n > 0) {
						while (n > 0) {
							char tmp[1024];
							int l = n;
							if (l > (int)sizeof(tmp)) {
								l = sizeof(tmp);
							}
							std::copy(inq.begin(), inq.begin() + l, tmp);
							if (fd_in_read != -1) {
								ssize_t r = write(fd_in_read, tmp, l);
								if (r < 0) {
									// 子プロセスが標準入力を閉じている（またはすでに終了した）。
									// これ以上書き込めないので入力側を閉じて諦める。
									closeInput();
									inq.clear();
									break;
								}
								inq.erase(inq.begin(), inq.begin() + r);
								n -= static_cast<int>(r);
								continue;
							}
							inq.clear();
							break;
						}
					} else if (close_input_later) {
						closeInput();
					}
				}
			}

			t1.wait();
			t2.wait();
		}

		close(fd_out_write);
		close(fd_err_write);
		pid = 0;
		return;

	fail:
		// ここに到達するのはpipe()/fork()がこのプロセス（親側）で失敗した場合のみ。
		// fdを使い果たした等の一時的な資源不足でホストアプリ全体を巻き込んで
		// 終了させるべきではないので、exit()は呼ばずに失敗として呼び出し元へ返す。
		if (stdin_pipe[R] >= 0) close(stdin_pipe[R]);
		if (stdin_pipe[W] >= 0) close(stdin_pipe[W]);
		if (stdout_pipe[R] >= 0) close(stdout_pipe[R]);
		if (stdout_pipe[W] >= 0) close(stdout_pipe[W]);
		if (stderr_pipe[R] >= 0) close(stderr_pipe[R]);
		if (stderr_pipe[W] >= 0) close(stderr_pipe[W]);
		fd_in_read = -1;
		pid = 0;
		exit_code = -1;
		// fprintf(stderr, "%s\n", error_message.c_str());
	}

public:
	ProcessPosixThread() = default;
	~ProcessPosixThread()
	{
		terminate();
		stop();
	}
	void writeInput(char const *ptr, int len)
	{
		if (!mutex || !ptr || len <= 0) return;
		std::lock_guard<std::mutex> lock(*mutex);
		inq.insert(inq.end(), ptr, ptr + len);
	}

	void closeInput()
	{
		if (fd_in_read >= 0) {
			close(fd_in_read);
			fd_in_read = -1;
		}
	}
	void start()
	{
		stop();
		{
			std::lock_guard<std::mutex> lock(finished_mutex);
			finished = false;
		}
		thread = std::thread([this]() {
			run();
			{
				std::lock_guard<std::mutex> lock(finished_mutex);
				finished = true;
			}
			finished_cv.notify_all();
		});
	}
	bool waitFor(int time)
	{
		if (!thread.joinable()) {
			return true;
		}
		std::unique_lock<std::mutex> lock(finished_mutex);
		if (time == INT_MAX) {
			finished_cv.wait(lock, [this]() { return finished; });
			return true;
		}
		return finished_cv.wait_for(lock, std::chrono::milliseconds(time), [this]() { return finished; });
	}
	void terminate()
	{
		pid_t child_pid = pid.load();
		if (child_pid > 0) {
			kill(child_pid, SIGTERM);
			// SIGTERM を無視する子のために SIGKILL へのエスカレーション期限を設定する
			auto dl = std::chrono::steady_clock::now() + std::chrono::seconds(2);
			term_deadline_ms = std::chrono::duration_cast<std::chrono::milliseconds>(dl.time_since_epoch()).count();
		}
		closeInput();
	}
	void stop()
	{
		if (thread.joinable()) {
			thread.join();
		}
	}
	void wait()
	{
		stop();
	}
};

struct ProcessPosix::Private {
	std::mutex mutex;
	ProcessPosixThread thread;
	std::vector<char> stdout_bytes;
	std::vector<char> stderr_bytes;
	int exit_code = -1;
	int error_code = 0;
	std::string error_message;
};

ProcessPosix::ProcessPosix()
	: m(new Private)
{
}

ProcessPosix::~ProcessPosix()
{
	delete m;
}

// コマンドライン文字列を空白で分割する簡易パーサ。
// - ダブルクォート内は空白を含めて1引数として扱う。
// - 連続する3つのダブルクォート (""" ) はリテラルの " 1個に展開する (独自仕様)。
// - 空の引数 ("" 等) は結果に含まれない。
void ProcessPosix::parse_args(std::string const &cmd, std::vector<std::string> *out)
{
	out->clear();
	char const *begin = cmd.c_str();
	char const *end = begin + cmd.size();
	std::vector<char> tmp;
	char const *ptr = begin;
	int quote = 0;
	while (1) {
		int c = 0;
		if (ptr < end) {
			c = *ptr & 0xff;
		}
		if (c == '\"' && ptr + 2 < end && ptr[1] == '\"' && ptr[2] == '\"') {
			tmp.push_back(c);
			ptr += 3;
		} else {
			if (quote != 0 && c != 0) {
				if (c == quote) {
					quote = 0;
				} else {
					tmp.push_back(c);
				}
			} else if (c == '\"') {
				quote = c;
			} else if (isspace(c) || c == 0) {
				if (!tmp.empty()) {
					std::string s(&tmp[0], tmp.size());
					out->push_back(s);
				}
				if (c == 0) break;
				tmp.clear();
			} else {
				tmp.push_back(c);
			}
			ptr++;
		}
	}
}

void ProcessPosix::start(std::string const &command, bool use_input)
{
	if (is_running()) return;
	m->exit_code = -1;
	m->error_code = 0;
	m->error_message.clear();
	parse_args(command, &m->thread.argvec);
	if (m->thread.argvec.empty()) {
		m->error_code = EINVAL;
		m->error_message = "empty command or failed to parse arguments";
		return;
	}
	for (std::string const &s : m->thread.argvec) {
		m->thread.args.push_back(const_cast<char *>(s.c_str()));
	}
	m->thread.args.push_back(nullptr);

	m->thread.init(&m->mutex, use_input);
	m->thread.start();
}

bool ProcessPosix::wait(int time)
{
	if (!m->thread.waitFor(time)) {
		return false;
	}
	m->thread.wait();

	m->stdout_bytes.clear();
	m->stderr_bytes.clear();
	if (!m->thread.outq.empty()) m->stdout_bytes.insert(m->stdout_bytes.end(), m->thread.outq.begin(), m->thread.outq.end());
	if (!m->thread.errq.empty()) m->stderr_bytes.insert(m->stderr_bytes.end(), m->thread.errq.begin(), m->thread.errq.end());
	m->exit_code = m->thread.exit_code;
	m->error_code = m->thread.error_code;
	m->error_message = std::move(m->thread.error_message);
	m->thread.reset();
	return true;
}

int ProcessPosix::get_error_code() const
{
	return m->error_code;
}

std::string const &ProcessPosix::get_error_message() const
{
	return m->error_message;
}

void ProcessPosix::write_input(char const *ptr, int len)
{
	m->thread.writeInput(ptr, len);
}

void ProcessPosix::close_input(bool justnow)
{
	if (justnow) {
		m->thread.closeInput();
	} else {
		m->thread.close_input_later = true;
	}
}

void ProcessPosix::close_input()
{
	close_input(true);
}

std::vector<char> const &ProcessPosix::stdout_bytes() const
{
	return m->stdout_bytes;
}

std::vector<char> const &ProcessPosix::stderr_bytes() const
{
	return m->stderr_bytes;
}

void ProcessPosix::stop()
{
	m->thread.terminate();
	wait();
}

bool ProcessPosix::is_running() const
{
	return m->thread.thread.joinable();
}

int ProcessPosix::get_exit_code() const
{
	return m->exit_code;
}

//

namespace {

void make_argv(char *command, std::vector<char *> *out)
{
	char *dst = command;
	char *src = command;
	char *arg = command;
	bool quote = false;
	bool accept = false;
	while (1) {
		char c = *src;
		if (c == '\"') {
			if (src[1] == '\"' && src[2] == '\"') {
				*dst++ = c;
				src += 3;
			} else {
				quote = !quote;
				accept = true;
				src++;
			}
		} else if (quote && c != 0) {
			*dst++ = *src++;
		} else if (c == 0 || isspace(c & 0xff)) {
			*dst++ = 0;
			if (accept || *arg) {
				out->push_back(arg);
			}
			if (c == 0) break;
			accept = false;
			arg = dst;
			src++;
		} else {
			*dst++ = *src++;
		}
	}
}

} // namespace

// ProcessPosixPty

struct ProcessPosixPty::Private {
	process::helper::PushDir pushd;
	std::atomic<bool> interrupted { false };
	std::mutex mutex;
	std::condition_variable cv;
	std::thread thread;
	bool completed = true;
	std::string command;
	std::string env;
	int pty_master = -1;
	int exit_code = -1;
	int error_code = 0;
	std::string error_message;
};

ProcessPosixPty::ProcessPosixPty()
	: m(new Private)
{
}

ProcessPosixPty::~ProcessPosixPty()
{
	stop();
	delete m;
}

bool ProcessPosixPty::is_running() const
{
	// return QThread::isRunning();
	return m->thread.joinable();
}

void ProcessPosixPty::write_input(char const *ptr, int len)
{
	if (!ptr || len <= 0) return;
	std::lock_guard<std::mutex> lock(m->mutex);
	if (m->pty_master < 0) return;
	while (len > 0) {
		ssize_t written = write(m->pty_master, ptr, static_cast<size_t>(len));
		if (written < 0 && errno == EINTR) continue;
		if (written <= 0) break;
		ptr += written;
		len -= static_cast<int>(written);
	}
}

int ProcessPosixPty::read_output(char *ptr, int len)
{
	// 出力バッファのロックは基底クラスの mutex_ に一元化されている
	return pop_output(ptr, len);
}

void ProcessPosixPty::start(std::string const &cmd, std::string const &env, bool use_input)
{
	(void)use_input;
	if (is_running()) return;
	m->command = cmd;
	m->env = env;
	m->interrupted = false;
	m->exit_code = -1;
	m->error_code = 0;
	m->error_message.clear();
	{
		std::lock_guard<std::mutex> lock(m->mutex);
		m->completed = false;
	}
	if (cmd.empty()) {
		m->error_code = EINVAL;
		m->error_message = "empty command";
		std::lock_guard<std::mutex> lock(m->mutex);
		m->completed = true;
		return;
	}
	// QThread::start();
	m->thread = std::thread([this]() {
		run();
		{
			std::lock_guard<std::mutex> lock(m->mutex);
			m->completed = true;
		}
		m->cv.notify_all();
	});
}

bool ProcessPosixPty::wait(int time)
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
	}
	return true;
}

int ProcessPosixPty::get_error_code() const
{
	return m->error_code;
}

std::string const &ProcessPosixPty::get_error_message() const
{
	return m->error_message;
}

void ProcessPosixPty::run()
{
	struct termios orig_termios = { };
	struct winsize orig_winsize = { 25, 80, 0, 0 };

	// TraceLogger trace;
	// trace.begin("process", QString::fromStdString(m->command));

	tcgetattr(STDIN_FILENO, &orig_termios);
	ioctl(STDIN_FILENO, TIOCGWINSZ, (char *)&orig_winsize);

	// argv は fork 前に構築する。fork 後の子プロセスで std::vector や文字列構築
	// (malloc) を行うと、親の他スレッドが保持しているロックとデッドロックしうる。
	std::vector<char> cmdbuf(m->command.begin(), m->command.end());
	cmdbuf.push_back('\0');
	std::vector<char *> argv;
	make_argv(cmdbuf.data(), &argv);
	if (argv.empty()) {
		m->error_code = EINVAL;
		m->error_message = "empty command";
		m->exit_code = -1;
		notify_completed();
		return;
	}
	argv.push_back(nullptr);
	// putenv は渡した文字列を保持するため、fork 前にコピーを確保しておく
	std::string envcopy = m->env;

	m->pty_master = posix_openpt(O_RDWR);
	if (m->pty_master < 0 || grantpt(m->pty_master) < 0 || unlockpt(m->pty_master) < 0) {
		// PTYを確保できない場合はforkせずに失敗として終了する。
		// ここでforkに進むと、壊れたfdを子プロセスに渡してしまい原因不明な不具合になる。
		m->error_code = errno;
		m->error_message = "failed: posix_openpt/grantpt/unlockpt";
		// fprintf(stderr, "%s\n", m->error_message.c_str());
		if (m->pty_master >= 0) {
			close(m->pty_master);
			m->pty_master = -1;
		}
		m->exit_code = -1;
		notify_completed();
		return;
	}

	pid_t pid = fork();
	if (pid < 0) {
		m->error_code = errno;
		m->error_message = "failed: fork";
		// fprintf(stderr, "%s\n", m->error_message.c_str());
		close(m->pty_master);
		m->pty_master = -1;
		m->exit_code = -1;
		notify_completed();
		return;
	}
	if (pid == 0) {
		// fork 後の子プロセス。malloc/stdio などロック取得を伴うAPIは
		// デッドロックの危険があるため最小限に留める。
		setsid();
		setenv("LANG", "C", 1);
		if (!envcopy.empty()) {
			putenv(envcopy.data());
		}

		char *pts_name = ptsname(m->pty_master);
		int pty_slave = pts_name ? open(pts_name, O_RDWR) : -1;
		close(m->pty_master);
		if (pty_slave < 0) {
			char const msg[] = "failed: open pty slave\n";
			if (write(STDERR_FILENO, msg, sizeof(msg) - 1) < 0) {
				// ignore
			}
			_exit(127);
		}

		struct termios tio;
		memset(&tio, 0, sizeof(tio));
		cfmakeraw(&tio);
		tio.c_cc[VMIN] = 1;
		tio.c_cc[VTIME] = 0;
		tio.c_lflag |= ECHO;
		tcsetattr(pty_slave, TCSANOW, &tio);
		ioctl(pty_slave, TIOCSWINSZ, &orig_winsize);

		dup2(pty_slave, STDIN_FILENO);
		dup2(pty_slave, STDOUT_FILENO);
		dup2(pty_slave, STDERR_FILENO);
		close(pty_slave);

		if (!change_dir_.empty()) {
			chdir(change_dir_.c_str());
		}

		execvp(argv[0], argv.data());

		// execvp()は成功すれば戻らない。ここに来るのは失敗した場合のみ。
		// 何もせず抜けると、フォークされたこの子プロセスがスレッド関数の
		// 残りを実行し続け（このプロセスにとっては唯一のスレッドなので）
		// 最終的に終了コード0で静かに終了してしまい、呼び出し元からは
		// コマンドが成功したように見えてしまう。
		{
			char const msg[] = "failed: exec\n";
			if (write(STDERR_FILENO, msg, sizeof(msg) - 1) < 0) {
				// ignore
			}
		}
		_exit(127);

	} else {

		bool ok = false;
		bool child_reaped = false;

		while (1) {
			// if (isInterruptionRequested()) break;
			if (m->interrupted) break;
			int status = 0;
			int r = waitpid(pid, &status, WNOHANG);
			if (r < 0) break;
			if (r > 0) {
				child_reaped = true;
				if (WIFEXITED(status)) {
					m->exit_code = WEXITSTATUS(status);
					ok = true;
				} else if (WIFSIGNALED(status)) {
					m->exit_code = 128 + WTERMSIG(status);
				}
				// 子が終了してもPTYバッファに出力が残っている場合があるため、
				// master が EIO/EOF になるまで読み切ってから抜ける
				while (1) {
					fd_set fds;
					FD_ZERO(&fds);
					FD_SET(m->pty_master, &fds);
					timeval tv;
					tv.tv_sec = 0;
					tv.tv_usec = 10000;
					int sr = select(m->pty_master + 1, &fds, nullptr, nullptr, &tv);
					if (sr <= 0) break;
					char buf[1024];
					int len = read(m->pty_master, buf, sizeof(buf));
					if (len <= 0) break;
					write_output(buf, len);
				}
				break;
			}

			{
				fd_set fds;
				FD_ZERO(&fds);
				FD_SET(m->pty_master, &fds);
				timeval tv;
				tv.tv_sec = 0;
				tv.tv_usec = 10000;
				int r = select(m->pty_master + 1, &fds, nullptr, nullptr, &tv);
				if (r < 0) break;
				if (r > 0) {
					char buf[1024];
					int len = read(m->pty_master, buf, sizeof(buf));
					if (len > 0) {
						write_output(buf, len);
					}
				}
			}
		}

		if (!child_reaped) {
			// SIGTERM を無視する子のため、猶予時間後に SIGKILL へエスカレーションする
			kill(pid, SIGTERM);
			int status = 0;
			bool reaped = false;
			for (int i = 0; i < 200; i++) { // 最大2秒 (200 x 10ms)
				pid_t wr = waitpid(pid, &status, WNOHANG);
				if (wr == pid) {
					reaped = true;
					break;
				}
				if (wr < 0 && errno != EINTR) break;
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
			if (!reaped) {
				kill(pid, SIGKILL);
				while (waitpid(pid, &status, 0) < 0 && errno == EINTR) { }
			}
			if (WIFSIGNALED(status)) m->exit_code = 128 + WTERMSIG(status);
		}
		close(m->pty_master);
		m->pty_master = -1;

		// trace.end();

		notify_completed();

		(void)ok;
	}
}

void ProcessPosixPty::stop()
{
	m->interrupted = true;
	wait();
}

int ProcessPosixPty::get_exit_code() const
{
	return m->exit_code;
}

void ProcessPosixPty::close_input()
{
}
