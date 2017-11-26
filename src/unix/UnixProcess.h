#ifndef UNIXPROCESS_H
#define UNIXPROCESS_H

#include <vector>
#include <string>
#include <QString>
#include <vector>
#include <deque>
#include <list>
#include "MyProcess.h"
#include <QThread>

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

	void exec(QString const &command);
	bool step(bool delay);

	int readOutput(char *dstptr, int maxlen);
	void writeInput(char const *ptr, int len);
	void closeInput();
	void stop();
};


class UnixProcess3 : public QThread {
private:
	struct Private;
	Private *m;

	class Task {
	public:
		std::string command;
		bool done = false;
		int exit_code = -1;
	};
protected:
	void run();
public:
	UnixProcess3();
	~UnixProcess3();

	void stop();
	int writeInput(char const *ptr, int len);
	int readOutput(char *ptr, int len);
	bool step(bool delay);
	void exec(QString const &command);
};

#endif // UNIXPROCESS_H
