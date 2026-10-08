#include "ProcessHardening.h"

#if defined(__unix__) || defined(__APPLE__)
#include <sys/resource.h>
#endif
#if defined(__linux__)
#include <sys/prctl.h>
#endif

namespace localvault {

bool hardenProcess()
{
	bool ok = true;
#if defined(__unix__) || defined(__APPLE__)
	const struct rlimit noCore = { 0, 0 };
	ok = setrlimit(RLIMIT_CORE, &noCore) == 0 && ok;
#endif
#if defined(__linux__)
	ok = prctl(PR_SET_DUMPABLE, 0, 0, 0, 0) == 0 && ok;
#endif
	return ok;
}

} // namespace localvault
