#include "Dir.h"
#include "misc.h"
#include <filesystem>
#include "common/unicode_conversion.h"

#ifdef _WIN32
#include <Windows.h>
#define PATH_MAX MAX_PATH
#else
#include <limits.h>
#include <unistd.h>
#endif

Dir::Dir(std::string const &path)
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

bool Dir::setCurrent(std::string const &path)
{
#ifdef _WIN32
	std::filesystem::path p = convert_utf8_to_utf16(path);
	return SetCurrentDirectoryW(p.c_str()) != 0;
#else
	return chdir(path.c_str()) == 0;
#endif
}

std::string Dir::currentPath()
{
	return current().path();
}
