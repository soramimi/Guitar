#include "Dir.h"
#include <unistd.h>
#include <limits.h>

Dir::Dir(const std::string &path)
	: path_(path)
{
}

std::string Dir::path() const
{
	return path_;
}

Dir Dir::current()
{
	char buf[PATH_MAX];
	if (getcwd(buf, sizeof(buf))) {
		return Dir(buf);
	}
	return {};
}

bool Dir::setCurrent(const std::string &path)
{
	return chdir(path.c_str()) == 0;
}
