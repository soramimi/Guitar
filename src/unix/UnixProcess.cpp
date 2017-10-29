#include "UnixProcess.h"
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <QDebug>
#include <QThread>


namespace {
class ReadThread : public QThread {
private:
	int fd;
	QByteArray *out;
protected:
	void run()
	{
		while (1) {
			char buf[256];
			int n = read(fd, buf, sizeof(buf));
			if (n < 1) break;
			if (out) out->append(buf, n);
		}
	}
public:
	ReadThread(int fd, QByteArray *out)
		: fd(fd)
		, out(out)
	{

	}
};
}
int UnixProcess::run(const char *file, char * const *argv, QByteArray *out, QByteArray *err)
{
	int exit_code = -1;
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

		ReadThread t1(fd_out_write, out);
		ReadThread t2(fd_err_write, err);
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

	return exit_code;
}

int UnixProcess::run(const QString &command, QByteArray *out, QByteArray *err)
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
		exit_code = run(args[0], &args[0], &outvec, &errvec);
		if (out) {
			out->clear();
			out->append(outvec);
		}
		if (err) {
			err->clear();
			err->append(errvec);
		}
	}

	return exit_code;
}

QString UnixProcess::errstring()
{
	return QString::fromUtf8(errvec);
}

