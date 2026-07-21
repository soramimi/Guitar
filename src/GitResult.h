
#ifndef GITRESULT_H
#define GITRESULT_H

#include "ProcessStatus.h"
#include <vector>

class GitResult {
private:
	ProcessStatus status_;
public:
	void set_exit_code(int code)
	{
		status_.exit_code = code;
	}
	void set_output(std::vector<char> const &out)
	{
		status_.output = out;
	}
	void set_error_message(std::string const &msg)
	{
		status_.error_message = msg;
	}

	bool ok() const
	{
		return status_.ok;
	}
	int exit_code()
	{
		return status_.exit_code;
	}
	std::vector<char> const &output() const
	{
		return status_.output;
	}
	std::string output_string() const
	{
		return std::string(status_.output.data(), status_.output.size());
	}
	std::string error_message() const
	{
		return status_.error_message;
	}
	std::string log_message() const
	{
		return status_.log_message;
	}
};


#endif // GITRESULT_H
