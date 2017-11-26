#ifndef WIN32PROCESS_H
#define WIN32PROCESS_H

#include <vector>
#include <string>
#include <stdint.h>
#include <QByteArray>
#include <vector>
#include <list>
#include <QThread>
#include "MyProcess.h"

class Win32Process : public AbstractProcess {
public:
	std::vector<char> outbytes;
	std::vector<char> errbytes;

	QString outstring() const;
	QString errstring() const;

	int run(QString const &command, bool use_input);
};

class Win32Process2 {
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
	Win32Process2();
	~Win32Process2();

	void start(bool use_input);
	void exec(const QString &command);
	bool step(bool delay);

	int readOutput(char *dstptr, int maxlen);
	void writeInput(char const *ptr, int len);
	void closeInput();
	void stop();
};


class Win32Process3 : public QThread {
private:
	struct Private;
	Private *m;

	static QString getProgram(QString const &cmdline);

	void close();
protected:
	void run();
public:
	Win32Process3();
	~Win32Process3();
	int readOutput(char *dstptr, int maxlen);
	void writeInput(char const *ptr, int len);
	void start(QString const &cmdline);
	void stop();
	int wait();
	std::vector<char> const *result() const;
};


#endif // WIN32PROCESS_H
