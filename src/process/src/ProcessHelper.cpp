
#ifdef _WIN32
#include <windows.h>
#else
#include <limits.h>
#include <unistd.h>
#endif

#include "ProcessHelper.h"

void process::helper::PushDir::chdir(dir_string_t const &dir)
{
#ifdef _WIN32
	if (!dir.empty()) {
		SetCurrentDirectoryW(dir.c_str());
	}
#else
	if (!dir.empty()) {
		::chdir(dir.c_str());
	}
#endif
}

process::helper::PushDir::PushDir(dir_string_t const &dir)
{
	pushd(dir);
}

process::helper::PushDir::~PushDir()
{
	popd();
}

void process::helper::PushDir::pushd(dir_string_t const &dir)
{
#ifdef _WIN32
	wchar_t tmp[MAX_PATH];
	if (GetCurrentDirectoryW(MAX_PATH, tmp) > 0) {
		cwd_ = tmp;
	}
#else
	char tmp[PATH_MAX];
	if (getcwd(tmp, sizeof(tmp))) {
		cwd_ = tmp;
	}
#endif
	chdir(dir);
}

void process::helper::PushDir::popd()
{
	chdir(cwd_);
	cwd_ = { };
}
