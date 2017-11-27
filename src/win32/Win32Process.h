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

#endif // WIN32PROCESS_H
