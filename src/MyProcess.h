#ifndef MYPROCESS_H
#define MYPROCESS_H

// Process.h というファイルはWindows環境に既存なので MyProcess.h にした

#ifdef _WIN32

#include <ProcessWin.h>
#include <ProcessWinPty.h>
#include <ProcessWinConPty.h>
#include <ProcessWinConPtyWithWorker.h>
using Process = ProcessWin;
// using PtyProcess = ProcessWinConPtyWithWorker;

std::shared_ptr<AbstractPtyProcess> new_winpty();
std::shared_ptr<AbstractPtyProcess> new_conpty();
std::shared_ptr<AbstractPtyProcess> new_conpty_with_worker_process();

#else

#include <BasicProcessPosix.h>
using Process = ProcessPosix;
using PtyProcess = ProcessPosixPty;

std::shared_ptr<AbstractPtyProcess> new_posix_pty();

#endif

#endif // MYPROCESS_H
