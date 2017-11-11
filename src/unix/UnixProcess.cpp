#include "UnixProcess.h"
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <QDebug>
#include <QThread>
#include <deque>

namespace {
class ReadThread : public QThread {
private:
	int fd;
	std::deque<char> *out;
protected:
	void run()
	{
		while (1) {
			char buf[256];
			int n = read(fd, buf, sizeof(buf));
			if (n < 1) break;
			if (out) {
				out->insert(out->end(), buf, buf + n);
			}
		}
	}
public:
	ReadThread(int fd, std::deque<char> *out)
		: fd(fd)
		, out(out)
	{

	}
};

class UnixProcessThread : public QThread {
public:
	const char *file;
	char * const *argv;
	std::deque<char> out;
	std::deque<char> err;
	AbstractProcess::stdinput_fn_t stdinput;
	int pid = 0;
	int exit_code = -1;
protected:
public:
	UnixProcessThread(const char *file, char * const *argv, AbstractProcess::stdinput_fn_t stdinput)
		: file(file)
		, argv(argv)
		, stdinput(stdinput)
	{
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
			int fd_in_read;
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

			close(fd_in_read);

			ReadThread t1(fd_out_write, &out);
			ReadThread t2(fd_err_write, &err);
			t1.start();
			t2.start();

			int status = 0;
			if (waitpid(pid, &status, 0) == pid) {
				if (WIFEXITED(status)) {
					exit_code = WEXITSTATUS(status);
				}
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
};

} // namespace

int UnixProcess::run(const char *file, char * const *argv, std::deque<char> *out, std::deque<char> *err, stdinput_fn_t stdinput)
{
	return 0;
}

int UnixProcess::run(const QString &command, stdinput_fn_t stdinput)
{
	int exit_code = -1;
	std::string cmd = command.toStdString();
	std::vector<std::string> vec;
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
					vec.push_back(s);
				}
				if (c == 0) break;
				tmp.clear();
			} else {
				tmp.push_back(c);
			}
			ptr++;
		}
	}
	if (vec.size() > 0) {
		std::vector<char *> args;
		for (std::string const &s : vec) {
			args.push_back(const_cast<char *>(s.c_str()));
		}
		args.push_back(nullptr);
//		exit_code = run(args[0], &args[0], &outbytes, &errbytes, stdinput);

		UnixProcessThread th(args[0], &args[0], stdinput);
		th.start();
		th.wait();
		exit_code = th.exit_code;

//		if (out) {
//			out->clear();
//			out->insert(out->end(), outbytes.begin(), outbytes.end());
//		}
//		if (err) {
//			err->clear();
//			err->insert(err->end(), errbytes.begin(), errbytes.end());
//		}
		outbytes.clear();
		errbytes.clear();
		if (!th.out.empty()) outbytes.insert(outbytes.end(), th.out.begin(), th.out.end());
		if (!th.err.empty()) errbytes.insert(errbytes.end(), th.err.begin(), th.err.end());
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

