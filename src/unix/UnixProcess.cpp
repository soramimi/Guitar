#include "UnixProcess.h"
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <QDebug>
#include <QMutex>
#include <QThread>
#include <deque>

namespace {
class ReadThread : public QThread {
private:
	int fd;
	QMutex *mutex;
	std::deque<char> *buffer;
protected:
	void run()
	{
		while (1) {
			char buf[256];
			int n = read(fd, buf, sizeof(buf));
			if (n < 1) break;
			if (buffer) {
				QMutexLocker lock(mutex);
				buffer->insert(buffer->end(), buf, buf + n);
			}
		}
	}
public:
	ReadThread(int fd, QMutex *mutex, std::deque<char> *out)
		: fd(fd)
		, mutex(mutex)
		, buffer(out)
	{

	}
};

class WriteThread : public QThread {
private:
	int fd;
	QMutex *mutex;
	std::deque<char> *buffer;
protected:
	void run()
	{
		while (1) {
			if (isInterruptionRequested()) return;

			int n = buffer->size();
			while (n > 0) {
				char buf[256];
				int l = n;
				if (l > sizeof(buf)) {
					l = sizeof(buf);
				}
				std::copy(buffer->begin(), buffer->begin() + l, buf);
				buffer->erase(buffer->begin(), buffer->begin() + l);
				write(fd, buf, l);
				n -= l;
			}
			msleep(1);
		}
	}
public:
	WriteThread(int fd, QMutex *mutex, std::deque<char> *in)
		: fd(fd)
		, mutex(mutex)
		, buffer(in)
	{

	}
};

class UnixProcessThread : public QThread {
	friend class ::UnixProcess2;
public:
	QMutex mutex;
	const char *file;
	char * const *argv;
	std::deque<char> input;
	std::deque<char> outvec;
	std::deque<char> errvec;
	bool use_input = false;
	int fd_in_read = -1;
	int pid = 0;
	int exit_code = -1;
protected:
public:
	UnixProcessThread(const char *file, char * const *argv, bool use_input)
		: file(file)
		, argv(argv)
		, use_input(use_input)
	{
	}
	UnixProcessThread()
	{
	}
	void init(const char *file, char * const *argv)
	{
		this->file = file;
		this->argv = argv;
	}
protected:
	void run()
	{
		exit_code = -1;
		const int R = 0;
		const int W = 1;
		const int E = 2;
		int stdin_pipe[2] = { -1, -1 };
		int stdout_pipe[2] = { -1, -1 };
		int stderr_pipe[2] = { -1, -1 };

		try {
			int fd_out_write;
			int fd_err_write;
			int pid;

			if (pipe(stdin_pipe) < 0) {
				throw std::string("failed: pipe");
			}

			if (pipe(stdout_pipe) < 0) {
				throw std::string("failed: pipe");
			}

			if (pipe(stderr_pipe) < 0) {
				throw std::string("failed: pipe");
			}

			pid = fork();
			if (pid < 0) {
				throw std::string("failed: fork");
			}

			if (pid == 0) { // child
				close(stdin_pipe[W]);
				close(stdout_pipe[R]);
				close(stderr_pipe[R]);
				dup2(stdin_pipe[R], R);
				dup2(stdout_pipe[W], W);
				dup2(stderr_pipe[W], E);
				close(stdin_pipe[R]);
				close(stdout_pipe[W]);
				close(stderr_pipe[E]);
				if (execvp(file, argv) < 0) {
					close(stdin_pipe[R]);
					close(stdout_pipe[W]);
					close(stderr_pipe[E]);
					fprintf(stderr, "failed: exec\n");
					exit(1);
				}
			}

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

			WriteThread t0(fd_in_read, &mutex, &input);
			ReadThread t1(fd_out_write, &mutex, &outvec);
			ReadThread t2(fd_err_write, &mutex, &errvec);
			if (use_input) t0.start();
			t1.start();
			t2.start();

			int status = 0;
			if (waitpid(pid, &status, 0) == pid) {
				if (WIFEXITED(status)) {
					exit_code = WEXITSTATUS(status);
				}
			}

			if (use_input) {
				t0.requestInterruption();
				t0.wait();
			}
			t1.wait();
			t2.wait();

			close(fd_out_write);

		} catch (std::string const &e) {
			close(stdin_pipe[R]);
			close(stdin_pipe[W]);
			close(stdout_pipe[R]);
			close(stdout_pipe[W]);
			close(stderr_pipe[R]);
			close(stderr_pipe[W]);
			fprintf(stderr, "%s\n", e.c_str());
			exit(1);
		}
	}

	void writeInput(const char *ptr, int len)
	{
		if (use_input && fd_in_read >= 0) {
			write(fd_in_read, ptr, len);
		}
	}

	void closeInput()
	{
		if (fd_in_read >= 0) {
			close(fd_in_read);
			fd_in_read = -1;
		}
	}
};

} // namespace



void UnixProcess::parseArgs(std::string const &cmd, std::vector<std::string> *out)
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

int UnixProcess::run(const QString &command, bool use_input)
{
	int exit_code = -1;
	std::string cmd = command.toStdString();
	std::vector<std::string> vec;
	parseArgs(cmd, &vec);
	if (vec.size() > 0) {
		std::vector<char *> args;
		for (std::string const &s : vec) {
			args.push_back(const_cast<char *>(s.c_str()));
		}
		args.push_back(nullptr);

		UnixProcessThread th(args[0], &args[0], use_input);
		th.start();
		th.wait();
		exit_code = th.exit_code;

		outbytes.clear();
		errbytes.clear();
		if (!th.outvec.empty()) outbytes.insert(outbytes.end(), th.outvec.begin(), th.outvec.end());
		if (!th.errvec.empty()) errbytes.insert(errbytes.end(), th.errvec.begin(), th.errvec.end());
	}

	return exit_code;
}

QString UnixProcess::errstring()
{
	if (errbytes.empty()) return QString();
	std::vector<char> v;
	v.insert(v.end(), errbytes.begin(), errbytes.end());
	return QString::fromUtf8(&v[0], v.size());
}


// experiment
//

struct UnixProcess2::Private {
	UnixProcessThread th;
	std::vector<std::string> args1;
	std::vector<char *> args2;
	std::list<UnixProcess2::Task> tasks;
};

UnixProcess2::UnixProcess2()
	: m(new Private)
{

}

UnixProcess2::~UnixProcess2()
{
	delete m;
}



void UnixProcess2::exec(const QString &command)
{
	m->th.use_input = true;

	Task task;
	task.command = command.toStdString();
	m->tasks.push_back(task);
	step(false);
}

bool UnixProcess2::step(bool delay)
{
	if (delay) {
		m->th.wait(1);
	}
	if (m->th.isRunning()) {
		return true;
	}
	if (!m->tasks.empty()) {
		Task task = m->tasks.front();
		m->tasks.pop_front();

		m->args1.clear();
		m->args2.clear();

		UnixProcess::parseArgs(task.command, &m->args1);
		if (m->args1.size() < 1) return false;

		for (std::string const &s : m->args1) {
			m->args2.push_back(const_cast<char *>(s.c_str()));
		}
		m->args2.push_back(nullptr);

		m->th.init(m->args2[0], &m->args2[0]);
		m->th.start();
		return true;
	}
	return false;
}

int UnixProcess2::readOutput(char *dstptr, int maxlen)
{
	QMutexLocker lock(&m->th.mutex);
	int pos = 0;
	while (pos < maxlen) {
		int n;
		n = m->th.outvec.size();
		if (n > 0) {
			if (n > maxlen) {
				n = maxlen;
			}
			for (int i = 0; i < n; i++) {
				dstptr[pos++] = m->th.outvec.front();
				m->th.outvec.pop_front();

			}

		} else {
			n = m->th.errvec.size();
			if (n > 0) {
				if (n > maxlen) {
					n = maxlen;
				}
				for (int i = 0; i < n; i++) {
					dstptr[pos++] = m->th.errvec.front();
					m->th.errvec.pop_front();
				}
			} else {
				break;
			}
		}
	}
	return pos;
}

void UnixProcess2::writeInput(const char *ptr, int len)
{
	QMutexLocker lock(&m->th.mutex);
	m->th.input.insert(m->th.input.end(), ptr, ptr + len);
}

void UnixProcess2::closeInput()
{
	m->th.closeInput();
}

void UnixProcess2::stop()
{
	if (m->th.isRunning()) {
		m->th.requestInterruption();
		closeInput();
		m->th.wait();
	}
}










