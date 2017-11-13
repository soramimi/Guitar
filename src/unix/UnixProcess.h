#ifndef UNIXPROCESS_H
#define UNIXPROCESS_H

#include <vector>
#include <string>
#include <QString>
#include <vector>
#include <deque>
#include "MyProcess.h"

class UnixProcess : public AbstractProcess {
private:
	int run(char const *file, char * const *argv, std::deque<char> *out, std::deque<char> *err, stdinput_fn_t stdinput);
public:
	int run(QString const &command, stdinput_fn_t stdinput = stdinput_fn_t());

	std::vector<char> outbytes;
	std::vector<char> errbytes;

	QString errstring();

	static void parseArgs(const std::string &cmd, std::vector<std::string> *vec);
};

class UnixProcess2 {
private:
	struct Private;
	Private *m;
public:
	UnixProcess2();
	~UnixProcess2();

	void start(QString const &command, AbstractProcess::stdinput_fn_t stdinput = AbstractProcess::stdinput_fn_t());
//	bool wait();

	int read(char *dstptr, int maxlen);
	bool step();
};

#endif // UNIXPROCESS_H
