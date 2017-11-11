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
	std::deque<char> outvec;
	std::deque<char> errvec;
	int run(char const *file, char * const *argv, std::deque<char> *out, std::deque<char> *err, stdinput_fn_t stdinput);
public:
	int run(QString const &command, std::vector<char> *out, std::vector<char> *err, stdinput_fn_t stdinput = stdinput_fn_t());
	QString errstring();
};

#endif // UNIXPROCESS_H
