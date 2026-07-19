#include "MyProcess.h"


#ifdef _WIN32

std::shared_ptr<AbstractPtyProcess> new_conpty_directly()
{
	return std::make_shared<ProcessWinPty>();
}

std::shared_ptr<AbstractPtyProcess> new_conpty_with_worker_process()
{
	return std::make_shared<ProcessConPtyWithWorker>();
}

std::shared_ptr<AbstractPtyProcess> new_winpty()
{
	return std::make_shared<ProcessWinPty>();
}

#else

std::shared_ptr<AbstractPtyProcess> new_posix_pty()
{
	return std::make_shared<ProcessPosixPty>();
}


#endif