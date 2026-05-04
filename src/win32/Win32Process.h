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

	Win32Process();
	~Win32Process();

	void start(const std::string &command, bool use_input);
	int wait();
	bool isRunning() const;
	void writeInput(char const *ptr, int len);
	void closeInput(bool justnow);

	void stop();
	int getExitCode() const;

	const std::vector<char> &stdout_bytes() const;
	const std::vector<char> &stderr_bytes() const;
};

#endif // WIN32PROCESS_H
