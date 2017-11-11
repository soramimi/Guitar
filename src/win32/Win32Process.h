#ifndef WIN32PROCESS_H
#define WIN32PROCESS_H

#include <vector>
#include <string>
#include <stdint.h>
#include <QByteArray>
#include <deque>
#include "MyProcess.h"

class Win32Process : public AbstractProcess {
private:
	std::deque<char> outvec;
	std::deque<char> errvec;
	stdinput_fn_t stdinput_callback_fn;
	uint32_t run(std::string const &command);
public:

	QString outstring() const;
	QString errstring() const;

	int run(QString const &command, std::vector<char> *out, std::vector<char> *err, stdinput_fn_t stdinput = stdinput_fn_t());
};

#endif // WIN32PROCESS_H
