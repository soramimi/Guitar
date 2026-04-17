#include "Dir.h"

#ifdef _WIN32
#include <Windows.h>
#define PATH_MAX MAX_PATH
#else
#include <limits.h>
#include <unistd.h>
#endif

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
#ifdef _WIN32
	if (GetCurrentDirectoryA(sizeof(buf), buf)) {
		return Dir(buf);
	}
#else
	if (getcwd(buf, sizeof(buf))) {
		return Dir(buf);
	}
#endif
	return {};
}

bool Dir::setCurrent(const std::string &path)
{
#ifdef _WIN32
	return SetCurrentDirectoryA(path.c_str()) != 0;
#else
	return chdir(path.c_str()) == 0;
#endif
}
