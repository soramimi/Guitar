#ifndef PROCESSWINCONPTYWITHWORKER_H
#define PROCESSWINCONPTYWITHWORKER_H
#include "AbstractProcess.h"
#include <memory>

// Lifecycle calls (start/stop/destruction) must be serialized by the caller.
// Completion runs on the reader thread: queue UI work and do not destroy/restart here.
class ProcessWinConPtyWithWorker : public AbstractPtyProcess {
	struct Private;
	std::unique_ptr<Private> m;
public:
	ProcessWinConPtyWithWorker();
	~ProcessWinConPtyWithWorker() override;
	static int run_worker(int argc, char **argv);
	void start(std::string const &command, std::string const &env, bool use_input) override;
	ProcessResult wait(int time = INT_MAX) override;
	void stop() override;
	bool is_running() const override;
	int get_exit_code() const override;
	void write_input(char const *ptr, int len) override;
	int read_output(char *ptr, int len) override;
	void close_input() override;
	void resize_terminal(int columns, int rows) override;
	bool wait_for_output(std::string const &text);
	// Configure before start; empty means conpty-worker.exe beside the host executable.
	void set_worker_path(process::helper::dir_string_t const &path);
};
#endif
