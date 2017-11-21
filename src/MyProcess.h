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

//	typedef std::function<bool (void *cookie)> stdinput_fn_t;

	virtual int run(QString const &command, bool use_input) = 0;

	static QString toQString(std::vector<char> const &vec);
};

#ifdef Q_OS_WIN
class Win32Process;
typedef Win32Process Process;
#else
class UnixProcess;
typedef UnixProcess Process;
#endif

#endif // MYPROCESS_H
