#ifndef MYPROCESS_H
#define MYPROCESS_H

// Process.h というファイルはWindows環境に既存なので MyProcess.h にした

#ifdef _WIN32

#include <ProcessWin.h>
#include <ProcessWinPty.h>
#include <ProcessWinConPtyWithWorker.h>
using Process = ProcessWin;
// using PtyProcess = ProcessWinPty;
using PtyProcess = ProcessWinConPtyWithWorker;

std::shared_ptr<AbstractPtyProcess> new_conpty_directly();
std::shared_ptr<AbstractPtyProcess> new_conpty_with_worker_process();
std::shared_ptr<AbstractPtyProcess> new_winpty();

#else

#include <BasicProcessPosix.h>
using Process = ProcessPosix;
using PtyProcess = ProcessPosixPty;

std::shared_ptr<AbstractPtyProcess> new_posix_pty();

#endif

class ProcessStatus {
public:
	bool ok = false;
	int exit_code = std::numeric_limits<int>::min();
	std::vector<char> output;
	std::string error_message;
	std::string log_message;
};

#endif // MYPROCESS_H
