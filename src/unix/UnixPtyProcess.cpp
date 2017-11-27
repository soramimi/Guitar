#include "UnixPtyProcess.h"
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <deque>
#include <QMutex>

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

class OutputReaderThread : public QThread {
private:
	QMutex *mutex;
	int pty_master;
	std::deque<char> *output_buffer;
protected:
	void run()
	{
		while (1) {
			if (isInterruptionRequested()) break;
			char buf[1024];
			int len = read(pty_master, buf, sizeof(buf));
			if (len < 1) break;
			{
				QMutexLocker lock(mutex);
				output_buffer->insert(output_buffer->end(), buf, buf + len);
			}
		}
	}
public:
	void start(QMutex *mutex, int pty_master, std::deque<char> *outbuf)
	{
		this->mutex = mutex;
		this->pty_master = pty_master;
		this->output_buffer = outbuf;
		QThread::start();
	}
};

} // namespace

struct UnixPtyProcess::Private {
	QMutex mutex;
	std::string command;
	int pty_master;
	std::deque<char> output_buffer;
	OutputReaderThread th_output_reader;
	int exit_code = -1;
};

UnixPtyProcess::UnixPtyProcess()
	: m(new Private)
{
}

UnixPtyProcess::~UnixPtyProcess()
{
	stop();
	delete m;
}

void UnixPtyProcess::writeInput(const char *ptr, int len)
{
	::write(m->pty_master, ptr, len);
}

int UnixPtyProcess::readOutput(char *ptr, int len)
{
	QMutexLocker lock(&m->mutex);
	int n = m->output_buffer.size();
	if (n > len) {
		n = len;
	}
	if (n > 0) {
		auto it = m->output_buffer.begin();
		std::copy(it, it + n, ptr);
		m->output_buffer.erase(it, it + n);
	}
	return n;
}

void UnixPtyProcess::start(const QString &cmd)
{
	m->command = cmd.toStdString();
	QThread::start();
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

		char *command = (char *)alloca(m->command.size() + 1);
		strcpy(command, m->command.c_str());
		std::vector<char *> argv;
		make_argv(command, &argv);
		argv.push_back(nullptr);
		execvp(argv[0], &argv[0]);

	} else {

		m->th_output_reader.start(&m->mutex, m->pty_master, &m->output_buffer);

		while (1) {
			if (isInterruptionRequested()) break;
			int status = 0;
			int r = waitpid(pid, &status, WNOHANG);
			if (r < 0) break;
			QThread::currentThread()->msleep(1);
			if (r > 0) {
				if (WIFEXITED(status)) {
					m->exit_code = WEXITSTATUS(status);
					break;
				}
				if (WIFSIGNALED(status)) {
					break;
				}
			}
		}

		kill(pid, SIGTERM);
		m->th_output_reader.requestInterruption();
		m->th_output_reader.wait();
		close(m->pty_master);
		m->pty_master = -1;
	}
}

void UnixPtyProcess::stop()
{
	requestInterruption();
	wait();
}


