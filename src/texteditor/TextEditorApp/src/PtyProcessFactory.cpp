#include "PtyProcessFactory.h"

#ifdef _WIN32
#include <ProcessWinConPty.h>
#include <ProcessWinConPtyWithWorker.h>
#else
#include <BasicProcessPosix.h>
#endif

std::shared_ptr<AbstractPtyProcess> createPtyProcess(PtyProcessBackend backend)
{
#ifdef _WIN32
	if (backend == PtyProcessBackend::Worker) {
		return std::make_shared<ProcessWinConPtyWithWorker>();
	}

	auto process = std::make_shared<ProcessWinConPty>();
	BasicProcessWinConPty::Options options;
	// ConPTY provides the terminal; CREATE_NO_WINDOW prevents output capture.
	options.no_window = false;
	options.output_queue = true;
	options.output_vector = false;
	options.vt_stripped = false;
	options.forward_parent_stdin = false;
	process->set_options(options);
	return process;
#else
	(void)backend;
	return std::make_shared<ProcessPosixPty>();
#endif
}
