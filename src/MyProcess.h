#ifndef MYPROCESS_H
#define MYPROCESS_H

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

#endif // MYPROCESS_H
