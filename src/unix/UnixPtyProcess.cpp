#include "UnixPtyProcess.h"
namespace {
//void parseArgs(const QString &cmd, QStringList *out)
//{
//	out->clear();
//	ushort const *begin = cmd.utf16();
//	ushort const *end = begin + cmd.size();
//	std::vector<ushort> tmp;
//	ushort const *ptr = begin;
//	ushort quote = 0;
//	while (1) {
//		int c = 0;
//		if (ptr < end) {
//			c = *ptr;
//		}
//		if (c == '\"' && ptr + 2 < end && ptr[1] == '\"' && ptr[2] == '\"') {
//			tmp.push_back(c);
//			ptr += 3;
//		} else {
//			if (quote != 0 && c != 0) {
//				if (c == quote) {
//					quote = 0;
//				} else {
//					tmp.push_back(c);
//				}
//			} else if (c == '\"') {
//				quote = c;
//			} else if (isspace(c) || c == 0) {
//				if (!tmp.empty()) {
//					QString s = QString::fromUtf16(&tmp[0], tmp.size());
//					out->push_back(s);
//				}
//				if (c == 0) break;
//				tmp.clear();
//			} else {
//				tmp.push_back(c);
//			}
//			ptr++;
//		}
//	}
//}
//void parseArgs(std::string const &cmd, std::vector<std::string> *out)
//{
//	out->clear();
//	char const *begin = cmd.c_str();
//	char const *end = begin + cmd.size();
//	std::vector<char> tmp;
//	char const *ptr = begin;
//	int quote = 0;
//	while (1) {
//		int c = 0;
//		if (ptr < end) {
//			c = *ptr & 0xff;
//		}
//		if (c == '\"' && ptr + 2 < end && ptr[1] == '\"' && ptr[2] == '\"') {
//			tmp.push_back(c);
//			ptr += 3;
//		} else {
//			if (quote != 0 && c != 0) {
//				if (c == quote) {
//					quote = 0;
//				} else {
//					tmp.push_back(c);
//				}
//			} else if (c == '\"') {
//				quote = c;
//			} else if (isspace(c) || c == 0) {
//				if (!tmp.empty()) {
//					std::string s(&tmp[0], tmp.size());
//					out->push_back(s);
//				}
//				if (c == 0) break;
//				tmp.clear();
//			} else {
//				tmp.push_back(c);
//			}
//			ptr++;
//		}
//	}
//}


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


}
//int UnixPtyProcess::start(const QString &program)
//{
//	QStringList argv;
//	QStringList env;
//	parseArgs(program, &argv);
//	int r = Pty::start(argv[0], argv, env, 0, 0);
//	waitForStarted();
//	return r;
//}

/////

//#include "PtyProcess.h"

#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <QDebug>
#include <QMutex>
#include <sys/wait.h>
#include <sys/types.h>
#include <deque>

namespace {
class OutputReaderThread2 : public QThread {
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

struct UnixPtyProcess2::Private {
	QMutex mutex;
	std::string command;
	int pty_master;
	std::deque<char> output_buffer;
	OutputReaderThread2 th_output_reader;
	int exit_code = -1;
};

UnixPtyProcess2::UnixPtyProcess2()
	: m(new Private)
{
}

UnixPtyProcess2::~UnixPtyProcess2()
{
	stop();
	delete m;
}

void UnixPtyProcess2::writeInput(const char *ptr, int len)
{
	qDebug() << QString::fromUtf8(ptr, len);
	::write(m->pty_master, ptr, len);
}

int UnixPtyProcess2::readOutput(char *ptr, int len)
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

void UnixPtyProcess2::start(const QString &cmd)
{
	m->command = cmd.toStdString();
	QThread::start();
}

void UnixPtyProcess2::run()
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

void UnixPtyProcess2::stop()
{
	requestInterruption();
	wait();
}


