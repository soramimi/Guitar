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
};

#endif // UNIXPROCESS_H
