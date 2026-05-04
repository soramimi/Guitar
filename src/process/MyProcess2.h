#ifndef MYPROCESS2_H
#define MYPROCESS2_H

#include <string>
#include <vector>

namespace process {

struct ProcessResult {
	bool ok = false;
	int exit_code = 0;

	operator bool () const
	{
		return ok;
	}
};


} // namespace process

namespace misc {
void parse_args(std::string const &cmd, std::vector<std::string> *out);
}


#ifdef _WIN32

#else
#include "ProcessPosix.h"
#endif



#endif // MYPROCESS2_H
