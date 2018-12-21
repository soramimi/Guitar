#ifndef WIN32PROCESS_H
#define WIN32PROCESS_H

#include <vector>
#include <string>
#include <stdint.h>
#include <QByteArray>
#include <vector>
#include <list>
#include <QThread>
//#include "MyProcess.h"

class Win32Process {
private:
	struct Private;
	Private *m;
public:
	std::vector<char> outbytes;
	std::vector<char> errbytes;

	Win32Process();
	~Win32Process();

	QString outstring() const;
	QString errstring() const;

	void start(QString const &command, bool use_input);
	int wait();
	void writeInput(char const *ptr, int len);
	void closeInput(bool justnow);
};

#endif // WIN32PROCESS_H
