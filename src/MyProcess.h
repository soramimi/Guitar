#ifndef MYPROCESS_H
#define MYPROCESS_H

// Process.h というファイルはWindows環境に既存なので MyProcess.h にした

#ifdef _WIN32

#include <ProcessWin.h>
#include <ProcessWinPty.h>
#include <ProcessConPtyWithWorker.h>
using Process = ProcessWin;
using PtyProcess = ProcessWinPty;

#else

#include <BasicProcessPosix.h>
using Process = ProcessPosix;
using PtyProcess = ProcessPosixPty;

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
