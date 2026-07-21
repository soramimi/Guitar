
#ifndef PROCESSWINPTY_H
#define PROCESSWINPTY_H
#include "AbstractProcess.h"

class ProcessWinPty : public AbstractPtyProcess {
private:
	struct Private;
	Private *m;
protected:
	void run();
public:
	ProcessWinPty();
	~ProcessWinPty() override;
	bool is_running() const override;
	int read_output(char *dstptr, int maxlen) override;
	void write_input(char const *ptr, int len) override;
	void close_input();
	void start(std::string const &cmdline, std::string const &env, bool use_input) override;
	void stop() override;
	ProcessResult wait(int time = INT_MAX) override;
	int get_exit_code() const override;
	std::string const &get_error_message() const;
};

#endif // PROCESSWINPTY_H
