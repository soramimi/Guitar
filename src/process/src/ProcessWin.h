#ifndef PROCESSWIN_H
#define PROCESSWIN_H

#include "AbstractProcess.h"
#include "BasicProcessWin.h"

class ProcessWin : public AbstractProcess {
private:
	struct Private;
	Private *m;
	void _close_input(bool justnow);

public:
	ProcessWin();
	~ProcessWin();
	bool is_running() const;
	void write_input(char const *ptr, int len);
	void close_input();
	void start(std::string const &command, bool use_input);
	void stop();
	ProcessResult wait(int time = INT_MAX);
	int get_exit_code() const;
	int get_error_code() const;
	std::string const &get_error_message() const;

	std::vector<char> const &stdout_bytes() const;
	std::vector<char> const &stderr_bytes() const;
};

#endif // PROCESSWIN_H
