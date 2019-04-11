#include "UnixProcess.h"
#include <QDebug>
#include <QMutex>
#include <QThread>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

class OutputReaderThread : public QThread {
private:
	int fd;
	QMutex *mutex;
	std::deque<char> *buffer;
protected:
	void run() override
	{
		while (1) {
			char buf[1024];
			int n = read(fd, buf, sizeof(buf));
			if (n < 1) break;
			if (buffer) {
				QMutexLocker lock(mutex);
				buffer->insert(buffer->end(), buf, buf + n);
			}
		}
	}
public:
	OutputReaderThread(int fd, QMutex *mutex, std::deque<char> *out)
		: fd(fd)
		, mutex(mutex)
		, buffer(out)
	{

	}
};

class UnixProcessThread : public QThread {
public:
	QMutex *mutex = nullptr;
	std::vector<std::string> argvec;
	std::vector<char *> args;
	std::deque<char> inq;
	std::deque<char> outq;
	std::deque<char> errq;
	bool use_input = false;
	int fd_in_read = -1;
	int pid = 0;
	int exit_code = -1;
	bool close_input_later = false;
protected:
public:
	void init(QMutex *mutex, bool use_input)
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
		exit_code = -1;
		close_input_later = false;
	}

protected:
	void run() override
	{
		exit_code = -1;
		const int R = 0;
		const int W = 1;
		const int E = 2;
		int stdin_pipe[3] = { -1, -1, -1 };
		int stdout_pipe[3] = { -1, -1, -1 };
		int stderr_pipe[3] = { -1, -1, -1 };

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

			OutputReaderThread t1(fd_out_write, mutex, &outq);
			OutputReaderThread t2(fd_err_write, mutex, &errq);
			t1.start();
			t2.start();

			while (1) {
				QThread::msleep(1);
				int status = 0;
				if (waitpid(pid, &status, WNOHANG) == pid) {
					if (WIFEXITED(status)) {
						exit_code = WEXITSTATUS(status);
						break;
					}
					if (WIFSIGNALED(status)) {
						exit_code = -1;
						break;
					}
				}
				{
					QMutexLocker lock(mutex);
					int n = inq.size();
					if (n > 0) {
						while (n > 0) {
							char tmp[1024];
							int l = n;
							if (l > (int)sizeof(tmp)) {
								l = sizeof(tmp);
							}
							std::copy(inq.begin(), inq.begin() + l, tmp);
							inq.erase(inq.begin(), inq.begin() + l);
							if (fd_in_read != -1) {
								int r = write(fd_in_read, tmp, l);
								(void)r;
							}
							n -= l;
						}
					} else if (close_input_later) {
						closeInput();
					}
				}
			}

			t1.wait();
			t2.wait();

			close(fd_out_write);
			close(fd_err_write);

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
public:
	void writeInput(char const *ptr, int len)
	{
		QMutexLocker lock(mutex);
		inq.insert(inq.end(), ptr, ptr + len);
	}

	void closeInput()
	{
		if (fd_in_read >= 0) {
			close(fd_in_read);
			fd_in_read = -1;
		}
	}
};

struct UnixProcess::Private {
	QMutex mutex;
	UnixProcessThread th;
};

UnixProcess::UnixProcess()
	: m(new Private)
{

}

UnixProcess::~UnixProcess()
{
	delete m;
}

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

void UnixProcess::start(QString const &command, bool use_input)
{
	std::string cmd = command.toStdString();
	parseArgs(cmd, &m->th.argvec);
	if (!m->th.argvec.empty()) {
		for (std::string const &s : m->th.argvec) {
			m->th.args.push_back(const_cast<char *>(s.c_str()));
		}
		m->th.args.push_back(nullptr);

		m->th.init(&m->mutex, use_input);
		m->th.start();
	}
}

int UnixProcess::wait()
{
	m->th.wait();

	outbytes.clear();
	errbytes.clear();
	if (!m->th.outq.empty()) outbytes.insert(outbytes.end(), m->th.outq.begin(), m->th.outq.end());
	if (!m->th.errq.empty()) errbytes.insert(errbytes.end(), m->th.errq.begin(), m->th.errq.end());
	int exit_code = m->th.exit_code;
	m->th.reset();
	return exit_code;
}

void UnixProcess::writeInput(char const *ptr, int len)
{
	m->th.writeInput(ptr, len);
}

void UnixProcess::closeInput(bool justnow)
{
	if (justnow) {
		m->th.closeInput();
	} else {
		m->th.close_input_later = true;
	}
}

QString UnixProcess::outstring()
{
	if (outbytes.empty()) return QString();
	std::vector<char> v;
	v.insert(v.end(), outbytes.begin(), outbytes.end());
	return QString::fromUtf8(&v[0], v.size());
}

QString UnixProcess::errstring()
{
	if (errbytes.empty()) return QString();
	std::vector<char> v;
	v.insert(v.end(), errbytes.begin(), errbytes.end());
	return QString::fromUtf8(&v[0], v.size());
}




