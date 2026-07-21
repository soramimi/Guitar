#ifndef BasicProcessWinConPty_H
#define BasicProcessWinConPty_H

#include "BasicProcessWin.h"

class BasicProcessWinConPty : public _AbstractBasicProcess {
private:
	struct Private;
	Private *m;

public:
	struct Options {
		bool no_window = false;
		bool output_stdout = false;
		bool output_vector = false;
		bool output_queue = false;
		bool vt_stripped = true;
	};

	BasicProcessWinConPty(Options const &options = Options());
	~BasicProcessWinConPty();
	void set_change_dir(process::helper::dir_string_t const &dir);
	void set_options(Options const &options);

	bool start(std::string const &cmd);
	ExecResult wait();
	void terminate();
	void close_input();
	int write_input(char const *ptr, int n);
	int read_output(char *ptr, int len);
	bool is_running() const;
	int get_exit_code() const;
	std::vector<char> const &stdout_bytes() const;

	void set_no_window(bool no_window);

	static bool is_conpty_available();
};

#endif // BasicProcessWinConPty_H
