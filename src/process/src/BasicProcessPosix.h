#ifndef BASICPROCESSPOSIX_H
#define BASICPROCESSPOSIX_H

#include "AbstractProcess.h"
#include <climits>
#include <optional>

class ProcessPosix : public AbstractProcess {
private:
	struct Private;
	Private *m;
	static void parse_args(std::string const &cmd, std::vector<std::string> *out);

public:
	ProcessPosix();
	~ProcessPosix();
	void start(std::string const &command, bool use_input);
	int wait();
	void stop();
	bool is_running() const;
	void write_input(char const *ptr, int len);
	void close_input();
	int get_exit_code() const;
	int get_error_code() const;
	std::string const &get_error_message() const;
	std::vector<char> const &stdout_bytes() const;
	std::vector<char> const &stderr_bytes() const;

	void close_input(bool justnow);
};

class ProcessPosixPty : public AbstractPtyProcess {
private:
	struct Private;
	Private *m;

protected:
	void run();

public:
	ProcessPosixPty();
	~ProcessPosixPty() override;
	bool is_running() const override;
	int read_output(char *ptr, int len) override;
	void write_input(char const *ptr, int len) override;
	void close_input();
	void start(std::string const &cmd, std::string const &env, bool use_input) override;
	int wait() override;
	void stop() override;
	int get_exit_code() const override;
	int get_error_code() const;
	std::string const &get_error_message() const;

	bool wait(unsigned long time);
};

#endif // BASICPROCESSPOSIX_H
