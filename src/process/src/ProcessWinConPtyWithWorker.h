#ifndef PROCESSWINCONPTYWITHWORKER_H
#define PROCESSWINCONPTYWITHWORKER_H

#include "AbstractProcess.h"
#include "BasicProcessWin.h"
#include "ProcessHelper.h"
#include <condition_variable>
#include <mutex>
#include <string>
#include <vector>

class ProcessWinConPtyWithWorker : public AbstractPtyProcess {
public:
	static int run_worker(int argc, char **argv);
	bool wait_for_output(std::string const &text);

private:
	constexpr static std::string_view subprocess_tag = "--conpty-worker--";

	struct Private;
	Private *m;

	int _wait();
public:
	ProcessWinConPtyWithWorker();
	~ProcessWinConPtyWithWorker() override;

	void set_options(BasicProcessWin::Options const &options);

	void start(std::string const &command, std::string const &env, bool use_input) override;
	bool wait(int time = INT_MAX) override;
	void stop() override;
	bool is_running() const override;
	int get_exit_code() const override;
	void write_input(char const *ptr, int len) override;
	int read_output(char *ptr, int len);
	void close_input() override;

};

#endif // PROCESSWINCONPTYWITHWORKER_H
