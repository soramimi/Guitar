#ifndef UNIXPROCESS_H
#define UNIXPROCESS_H

#include <vector>
#include <string>
#include <vector>
#include <optional>
#include "../AbstractProcess.h"

class UnixProcess : public AbstractProcess {
private:
	struct Private;
	Private *m;
	mutable std::vector<char> stdout_bytes_;
	mutable std::vector<char> stderr_bytes_;
	static void parseArgs(std::string const &cmd, std::vector<std::string> *out);
public:

	UnixProcess();
	~UnixProcess();

	void start(std::string const &command, bool use_input);
	int wait();
	void stop();
	bool isRunning() const;
	void writeInput(char const *ptr, int len);
	void closeInput(bool justnow);
	int getExitCode() const;
	std::vector<char> const &stdout_bytes() const;
	std::vector<char> const &stderr_bytes() const;

	static std::optional<std::string> run_and_wait(std::string const &command);


	// AbstractProcess interface
public:
};

#endif // UNIXPROCESS_H
