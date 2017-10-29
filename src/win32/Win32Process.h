#ifndef WIN32PROCESS_H
#define WIN32PROCESS_H

#include <vector>
#include <string>
#include <stdint.h>
#include <QByteArray>

class Win32Process {
private:
	QByteArray outvec;
	QByteArray errvec;
	uint32_t run(std::string const &command);
public:

	QString outstring() const;
	QString errstring() const;

	int run(QString const &command, QByteArray *out, QByteArray *err);
};

#endif // WIN32PROCESS_H
