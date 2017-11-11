#ifndef MYPROCESS_H
#define MYPROCESS_H

#include <QString>
#include <functional>
#include <vector>

class AbstractProcess {
public:
	virtual ~AbstractProcess()
	{
	}

	typedef std::function<bool (const char *, int)> stdinput_fn_t;

	virtual int run(QString const &command, std::vector<char> *out, std::vector<char> *err, stdinput_fn_t stdinput = stdinput_fn_t()) = 0;
};

#ifdef Q_OS_WIN
class Win32Process;
typedef Win32Process Process;
#else
class UnixProcess;
typedef UnixProcess Process;
#endif

#endif // MYPROCESS_H
