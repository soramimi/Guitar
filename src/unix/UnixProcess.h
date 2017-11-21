#ifndef UNIXPROCESS_H
#define UNIXPROCESS_H

#include <vector>
#include <string>
#include <QString>
#include <vector>
#include <deque>
#include "MyProcess.h"

class UnixProcess : public AbstractProcess {
public:
	int run(QString const &command, bool use_input);

	std::vector<char> outbytes;
	std::vector<char> errbytes;

	QString errstring();

	static void parseArgs(const std::string &cmd, std::vector<std::string> *out);
};

class UnixProcess2 {
public:
	class Task {
	public:
		std::string command;
		bool done = false;
		int exit_code = -1;
	};
private:
	struct Private;
	Private *m;
public:
	UnixProcess2();
	~UnixProcess2();

	void start(bool use_input);
	void exec(QString const &command);
	bool step(bool delay);

	int read(char *dstptr, int maxlen);
};

#endif // UNIXPROCESS_H
