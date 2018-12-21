#ifndef UNIXPROCESS_H
#define UNIXPROCESS_H

#include <vector>
#include <string>
#include <QString>
#include <vector>
#include <deque>
#include <list>

class UnixProcess {
private:
	struct Private;
	Private *m;
public:
	std::vector<char> outbytes;
	std::vector<char> errbytes;

	UnixProcess();
	~UnixProcess();

	QString outstring();
	QString errstring();

	static void parseArgs(const std::string &cmd, std::vector<std::string> *out);

	void start(QString const &command, bool use_input);
	int wait();
	void writeInput(char const *ptr, int len);
	void closeInput(bool justnow);
};

#endif // UNIXPROCESS_H
