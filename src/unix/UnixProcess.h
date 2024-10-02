#ifndef UNIXPROCESS_H
#define UNIXPROCESS_H

#include <vector>
#include <string>
#include <vector>
#include <optional>

class UnixProcess {
private:
	struct Private;
	Private *m;
public:
	std::vector<char> outbytes;
	std::vector<char> errbytes;

	UnixProcess();
	~UnixProcess();

	std::string outstring();
	std::string errstring();

	static void parseArgs(std::string const &cmd, std::vector<std::string> *out);

	void start(std::string const &command, bool use_input);
	int wait();
	void writeInput(char const *ptr, int len);
	void closeInput(bool justnow);

	static std::optional<std::string> run_and_wait(std::string const &command);
};

#endif // UNIXPROCESS_H
