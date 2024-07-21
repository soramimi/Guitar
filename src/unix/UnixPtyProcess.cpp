#include "UnixPtyProcess.h"
#include <QDir>
#include <QMutex>
#include <csignal>
#include <cstdlib>
#include <deque>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

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

// UnixPtyProcess

struct UnixPtyProcess::Private {
	QMutex mutex;
	std::string command;
	std::string env;
	int pty_master;
	std::deque<char> output_queue;
	std::vector<char> output_vector;
	int exit_code = -1;
};

UnixPtyProcess::UnixPtyProcess()
	: m(new Private)
{
}

UnixPtyProcess::~UnixPtyProcess()
{
	stop_();
	delete m;
}

bool UnixPtyProcess::isRunning() const
{
	return QThread::isRunning();
}

void UnixPtyProcess::writeInput(char const *ptr, int len)
{
	int r = write(m->pty_master, ptr, len);
	(void)r;
}

int UnixPtyProcess::readOutput(char *ptr, int len)
{
	QMutexLocker lock(&m->mutex);
	int n = m->output_queue.size();
	if (n > len) {
		n = len;
	}
	if (n > 0) {
		auto it = m->output_queue.begin();
		std::copy(it, it + n, ptr);
		m->output_queue.erase(it, it + n);
	}
	return n;
}

void UnixPtyProcess::start(QString const &cmd, QString const &env)
{
	if (isRunning()) return;
	m->command = cmd.toStdString();
	m->env = env.toStdString();
	QThread::start();
}

bool UnixPtyProcess::wait_(unsigned long time)
{
	return QThread::wait(time);
}

bool UnixPtyProcess::wait(unsigned long time)
{
	return wait_(time);
}

void UnixPtyProcess::run()
{
	struct termios orig_termios;
	struct winsize orig_winsize;

	tcgetattr(STDIN_FILENO, &orig_termios);
	ioctl(STDIN_FILENO, TIOCGWINSZ, (char *)&orig_winsize);

	m->pty_master = posix_openpt(O_RDWR);
	grantpt(m->pty_master);
	unlockpt(m->pty_master);

	pid_t pid = fork();
	if (pid == 0) {
		setsid();
		setenv("LANG", "C", 1);
		if (!m->env.empty()) {
			char *env = (char *)alloca(m->env.size() + 1);
			strcpy(env, m->env.c_str());
			putenv(env);
		}

		char *pts_name = ptsname(m->pty_master);
		int pty_slave = open(pts_name, O_RDWR);
		close(m->pty_master);

		struct termios tio;
		memset(&tio, 0, sizeof(tio));
		cfmakeraw(&tio);
		tio.c_cc[VMIN]  = 1;
		tio.c_cc[VTIME] = 0;
		tio.c_lflag |= ECHO;
		tcsetattr(pty_slave, TCSANOW, &tio);
		ioctl(pty_slave, TIOCSWINSZ, &orig_winsize);

		dup2(pty_slave, STDIN_FILENO);
		dup2(pty_slave, STDOUT_FILENO);
		dup2(pty_slave, STDERR_FILENO);
		close(pty_slave);

		QDir::setCurrent(change_dir_);

		char *command = (char *)alloca(m->command.size() + 1);
		strcpy(command, m->command.c_str());
		std::vector<char *> argv;
		make_argv(command, &argv);
		argv.push_back(nullptr);
		execvp(argv[0], &argv[0]);

	} else {

		bool ok = false;

		while (1) {
			if (isInterruptionRequested()) break;
			int status = 0;
			int r = waitpid(pid, &status, WNOHANG);
			if (r < 0) break;
			if (r > 0) {
				if (WIFEXITED(status)) {
					m->exit_code = WEXITSTATUS(status);
					ok = true;
					break;
				}
				if (WIFSIGNALED(status)) {
					break;
				}
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
						QMutexLocker lock(&m->mutex);
						m->output_queue.insert(m->output_queue.end(), buf, buf + len);
						m->output_vector.insert(m->output_vector.end(), buf, buf + len);
					}
				}
			}
		}

		kill(pid, SIGTERM);
		close(m->pty_master);
		m->pty_master = -1;

		if (completed_fn_) {
			completed_fn_(ok, user_data_);
		}
	}
}

void UnixPtyProcess::stop_()
{
	requestInterruption();
	wait_();
}

void UnixPtyProcess::stop()
{
	stop_();
}

int UnixPtyProcess::getExitCode() const
{
	return m->exit_code;
}

QString UnixPtyProcess::getMessage() const
{
	QString s;
	if (!m->output_vector.empty()) {
		s = QString::fromUtf8(&m->output_vector[0], m->output_vector.size());
	}
	return s;
}

void UnixPtyProcess::readResult(std::vector<char> *out)
{
	*out = m->output_vector;
	m->output_vector.clear();
}

