#ifndef WIN32PROCESS_H
#define WIN32PROCESS_H

#include <vector>
#include <string>
#include <QByteArray>
#include <vector>
#include <QThread>
#include "../AbstractProcess.h"

class Win32Process : public AbstractProcess {
private:
	struct Private;
	Private *m;
public:
	std::vector<char> outbytes;
	std::vector<char> errbytes;

	Win32Process();
	~Win32Process();

	std::string outstring() const;
	std::string errstring() const;

	void start(const std::string &command, bool use_input);
	int wait();
	void writeInput(char const *ptr, int len);
	void closeInput(bool justnow);

	void stop();
	int getExitCode() const;
};

#endif // WIN32PROCESS_H
