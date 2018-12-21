#ifndef MYPROCESS_H
#define MYPROCESS_H

#include <QtGlobal>

#ifdef Q_OS_WIN
#include "win32/Win32Process.h"
#include "win32/Win32PtyProcess.h"
typedef Win32Process Process;
typedef Win32PtyProcess PtyProcess;
#else
#include "unix/UnixProcess.h"
#include "unix/UnixPtyProcess.h"
typedef UnixProcess Process;
typedef UnixPtyProcess PtyProcess;
#endif

class misc2 {
public:
	static int runCommand(QString const &cmd, QByteArray *out);
	static int runCommand(QString const &cmd, const QByteArray *in, QByteArray *out);
};

#endif // MYPROCESS_H
