#ifndef WIN32PROCESS_H
#define WIN32PROCESS_H

#include <vector>
#include <string>
#include <stdint.h>
#include <QByteArray>

class Win32Process {
private:
	std::vector<char> outvec;
	std::vector<char> errvec;
	uint32_t run(std::string const &command);
public:

	std::string outstring() const;
	std::string errstring() const;

	int run(QString const &command, QByteArray *out, QByteArray *err);
};

#endif // WIN32PROCESS_H
