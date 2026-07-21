
#include <algorithm>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include "misc.h"
#include <BasicProcessWin.h>
#include <ProcessWinConPtyWithWorker.h>
#include <ProcessWin.h>
#include <ProcessWinConPty.h>
#include <ProcessWinPty.h>
#else
#include <BasicProcessPosix.h>
#endif

std::string trimmed(std::string str)
{
	str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](unsigned char ch) {
		return !std::isspace(ch);
	}));
	str.erase(std::find_if(str.rbegin(), str.rend(), [](unsigned char ch) {
		return !std::isspace(ch);
	}).base(), str.end());
	return str;
}

#ifdef _WIN32
int main_win_conpty_with_worker(std::string const &cmd)
{
	std::string ssh = find_windows_openssh();
	if (ssh.empty()) {
		fprintf(stderr, "Windows OpenSSH client was not found.\n");
		return 1;
	}

	ProcessWinConPtyWithWorker proc;
	proc.start(cmd, {}, true);

#if 1
	std::string prompt = "Are you sure you want to continue connecting";
	if (proc.wait_for_output(prompt)) {
		proc.write_input("no\n", 3);
	}
#endif

	proc.close_input();
	proc.wait();

	std::vector<char> const &v = proc.stdout_bytes();
	std::string str(v.data(), v.size());
	printf("[%s]\n", trimmed(str).c_str());
	return 0;
}

int main_win(std::string const &cmd)
{
	ProcessWin proc;
	proc.start(cmd, false);
	proc.wait();
	auto vec = proc.stdout_bytes();
	std::string_view view(vec.data(), vec.size());
	std::string str = std::string(view);
	printf("[%s]\n", trimmed(str).c_str());
	return 0;
}

int main_winpty(std::string const &cmd)
{
	ProcessWinPty proc;
	proc.start(cmd, {}, false);
	proc.wait();
	{
		char tmp[1024];
		int len = proc.read_output(tmp, sizeof(tmp) - 1);
		tmp[len] = 0;
		printf("[%s]\n", trimmed(tmp).c_str());
	}
	return 0;
}

int main_basic_win(std::string const &cmd)
{
	BasicProcessWin::Options opts;
	opts.output_vector = true;
	BasicProcessWin proc(opts);
	proc.start(cmd);
	proc.wait();
	auto vec = proc.stdout_bytes();
	std::string_view view(vec.data(), vec.size());
	std::string str = std::string(view);
	printf("[%s]\n", trimmed(str).c_str());
	return 0;
}

int main_basic_win_conpty(std::string const &cmd)
{
	BasicProcessWinConPty::Options opts;
	opts.output_vector = true;
	BasicProcessWinConPty proc(opts);
	proc.set_no_window(false);
	proc.start(cmd);
	auto result = proc.wait();
	auto vec = proc.stdout_bytes();
	std::string_view view(vec.data(), vec.size());
	std::string str = std::string(view);
	printf("[%s]\n", trimmed(str).c_str());
	return 0;
}


int main_win_conpty(std::string const &cmd)
{
	ProcessWinConPty proc;
	proc.start(cmd, {}, false);
	proc.wait();
	auto vec = proc.stdout_bytes();
	std::string_view view(vec.data(), vec.size());
	std::string str = std::string(view);
	printf("[%s]\n", trimmed(str).c_str());
	return 0;
}
#else
#endif

#ifdef _WIN32
#ifdef CONPTY_WORKER
int main(int argc, char **argv)
{
	int ret = ProcessWinConPtyWithWorker::run_worker(argc, argv);
	if (ret < 0) {
		fprintf(stderr, "Error: failed to run worker process\n");
	}
	return ret;
}
#else
int main(int argc, char **argv)
{
	std::string cmd = R"("C:\Program Files\Git\cmd\git.exe")";
	cmd += " --version";

	main_basic_win(cmd);
	main_basic_win_conpty(cmd);
	main_win(cmd);
	main_win_conpty(cmd);
	main_win_conpty_with_worker(cmd);
	main_winpty(cmd);

	return 0;
}
#endif
#else

int main_basic_posix(int /*argc*/, char ** /*argv*/)
{
	std::string cmd = R"("/usr/bin/git")";
	cmd += " --version";
	ProcessPosix proc;
	proc.start(cmd, false);
	proc.wait();
	auto vec = proc.stdout_bytes();
	std::string_view view(vec.data(), vec.size());
	std::string str = std::string(view);
	puts(trimmed(str).c_str());
	return 0;
}

int main_basic_posix_pty(int /*argc*/, char ** /*argv*/)
{
	std::string cmd = R"("/usr/bin/git")";
	cmd += " --version";
	ProcessPosixPty proc;
	proc.start(cmd, {}, false);
	proc.wait();
	auto vec = proc.stdout_bytes();
	std::string_view view(vec.data(), vec.size());
	std::string str = std::string(view);
	puts(trimmed(str).c_str());
	return 0;
}

int main(int argc, char **argv)
{
	main_basic_posix(argc, argv);
	main_basic_posix_pty(argc, argv);
	return 0;
}
#endif

