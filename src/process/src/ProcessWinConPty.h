
#ifndef PROCESSWINCONPTY_H
#define PROCESSWINCONPTY_H

#include "AbstractProcess.h"
#include "BasicProcessWinConPty.h"

class ProcessWinConPty : public AbstractPtyProcess {
private:
	struct Private;
	Private *m;

public:
	ProcessWinConPty();
	~ProcessWinConPty() override;
	void start(std::string const &command, std::string const &env, bool use_input) override;
	bool wait(int time = INT_MAX) override;
	void stop() override;
	bool is_running() const override;
	int get_exit_code() const override;
	void close_input() override;
	void write_input(char const *ptr, int len) override;
	int read_output(char *ptr, int len);
	void set_no_window(bool no_window);
	void set_options(BasicProcessWinConPty::Options const &options);
};

#endif // PROCESSWINCONPTY_H
