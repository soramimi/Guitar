#include "FileInfo.h"
#include <sys/stat.h>

struct FileInfo::Private {
	std::string file;
	bool valid;
	struct stat stat;
};

FileInfo::FileInfo(const std::string &file)
	: m(new Private)
{
	m->file = file;
	m->valid = stat(m->file.c_str(), &m->stat) == 0;
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
	return Dir(path);
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
