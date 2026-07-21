
#ifndef PROCESSSTATUS_H
#define PROCESSSTATUS_H

#include <string>
#include <vector>
#include <limits>

class ProcessStatus {
public:
	bool ok = false;
	int exit_code = std::numeric_limits<int>::min();
	std::vector<char> output;
	std::string error_message;
	std::string log_message;
};

#endif // PROCESSSTATUS_H
