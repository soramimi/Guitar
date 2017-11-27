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
public:
	int run(QString const &command, bool use_input);

	std::vector<char> outbytes;
	std::vector<char> errbytes;

	QString errstring();

	static void parseArgs(const std::string &cmd, std::vector<std::string> *out);
};

#endif // UNIXPROCESS_H
