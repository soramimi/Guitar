#include "FileInfo.h"
#include <common/misc.h>
#include <common/unicode_conversion.h>
#include <filesystem>
#include <sys/stat.h>

namespace misc {
std::string realpath(const char *path);
std::string realpath(std::string const &path);
}

struct FileInfo::Private {
	std::string file;
	bool valid = false;
#ifdef _WIN32
	struct _stat64 stat = {};
#else
	struct stat stat = {};
#endif
};

FileInfo::FileInfo()
	: m(new Private)
{
}

FileInfo::FileInfo(const std::string &file)
	: m(new Private)
{
	m->file = file;
	std::filesystem::path path(convert_utf8_to_utf16(file));
	path.make_preferred();
	std::filesystem::path::value_type const *cstr = path.c_str();
#ifdef _WIN32
	m->valid = _wstat64(cstr, &m->stat) == 0;
#else
	m->valid = stat(cstr, &m->stat) == 0;
#endif
}

FileInfo::~FileInfo()
{
	delete m;
}

bool FileInfo::isFile() const
{
#ifdef _WIN32
	return m->valid && (m->stat.st_mode & S_IFREG);
#else
	return m->valid && S_ISREG(m->stat.st_mode);
#endif
}

bool FileInfo::isDir() const
{
#ifdef _WIN32
	return m->valid && (m->stat.st_mode & S_IFDIR);
#else
	return m->valid && S_ISDIR(m->stat.st_mode);
#endif
}

bool FileInfo::isExecutable() const
{
#ifdef _WIN32
	return m->valid && (m->stat.st_mode & S_IEXEC);
#else
	return m->valid && (m->stat.st_mode & S_IXUSR);
#endif
}

Dir FileInfo::dir() const
{
	std::string path = m->file;
	size_t pos = path.find_last_of("/\\");
	if (pos != std::string::npos) {
		path = path.substr(0, pos);
	} else {
		path = ".";
	}
	return Dir(m->file);
}

std::string FileInfo::fileName() const
{
	std::string path = m->file;
	size_t pos = path.find_last_of("/\\");
	if (pos != std::string::npos) {
		return path.substr(pos + 1);
	} else {
		return path;
	}
}

std::string FileInfo::absoluteFilePath() const
{
	return misc::realpath(m->file);
}

