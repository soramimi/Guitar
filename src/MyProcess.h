#ifndef MYPROCESS_H
#define MYPROCESS_H

// Process.h というファイルはWindows環境に既存なので MyProcess.h にした

#include <QtGlobal>

#ifdef Q_OS_WIN
#include "win32/Win32Process.h"
#include "win32/Win32PtyProcess.h"
using Process = Win32Process;
using PtyProcess = Win32PtyProcess;
#else
#include "unix/UnixProcess.h"
#include "unix/UnixPtyProcess.h"
using Process = UnixProcess;
using PtyProcess = UnixPtyProcess;
#endif

class ProcessStatus {
public:
	bool ok = false;
	int exit_code = 0;
	QString error_message;
	QString log_message;
};
Q_DECLARE_METATYPE(ProcessStatus)

#endif // MYPROCESS_H
