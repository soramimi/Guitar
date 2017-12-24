#ifndef UNIXPROCESS_H
#define UNIXPROCESS_H

#include <vector>
#include <string>
#include <QString>
#include <vector>
#include <deque>
#include <list>
//#include "MyProcess.h"
//#include <QThread>

class UnixProcess {
private:
	struct Private;
	Private *m;
public:
	UnixProcess();
	~UnixProcess();
	void start(QString const &command, bool use_input);

	std::vector<char> outbytes;
	std::vector<char> errbytes;

	QString outstring();
	QString errstring();

	static void parseArgs(const std::string &cmd, std::vector<std::string> *out);

	int wait();

	void writeInput(const char *ptr, int len);

	void closeInput(bool justnow);
};

#endif // UNIXPROCESS_H
