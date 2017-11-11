#ifndef WIN32PROCESS_H
#define WIN32PROCESS_H

#include <vector>
#include <string>
#include <stdint.h>
#include <QByteArray>
#include <vector>
#include "MyProcess.h"

class Win32Process : public AbstractProcess {
public:
	std::vector<char> outbytes;
	std::vector<char> errbytes;

	QString outstring() const;
	QString errstring() const;

	int run(QString const &command, stdinput_fn_t stdinput = stdinput_fn_t());
};

class Win32Process2 {
private:
	struct Private;
	Private *m;
public:
	Win32Process2();
	~Win32Process2();

	void start(QString const &command, AbstractProcess::stdinput_fn_t stdinput = AbstractProcess::stdinput_fn_t());
	bool wait();

	int read(char *dstptr, int maxlen);
	bool step();
};

#endif // WIN32PROCESS_H
