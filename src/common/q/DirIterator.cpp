#include "DirIterator.h"

#include "common/joinpath.h"

#ifdef _WIN32
#include <Windows.h>

struct DirIterator::Private {
	std::string path;
	int filter = Dir::Dirs | Dir::Files;
	bool hasnext = false;
	WIN32_FIND_DATAA finddata = {};
	HANDLE handle = nullptr;
};

DirIterator::DirIterator(const std::string &path, int filter)
	: m(new Private)
{
	m->path = path;
	m->filter = filter;
}

DirIterator::~DirIterator()
{
	if (m->handle) {
		FindClose(m->handle);
	}
}

bool DirIterator::hasNext()
{
	if (m->hasnext) return true;
	if (!m->handle) {
		std::string filter = m->path / "*.*";
		m->handle = FindFirstFileA(filter.c_str(), &m->finddata);
		if (m->handle == INVALID_HANDLE_VALUE) {
			m->handle = nullptr;
			return false;
		}
	} else {
		if (!FindNextFileA(m->handle, &m->finddata)) {
			return false;
		}
	}
	do {
		if (strcmp(m->finddata.cFileName, ".") == 0) continue;
		if (strcmp(m->finddata.cFileName, "..") == 0) continue;
		if (m->finddata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			if (m->filter & Dir::Dirs) {
				m->hasnext = true;
				break;
			}
		} else {
			if (m->filter & Dir::Files) {
				m->hasnext = true;
				break;
			}
		}
	} while (FindNextFileA(m->handle, &m->finddata));
	return m->hasnext;
}

std::string DirIterator::next() const
{
	if (m->hasnext) {
		m->hasnext = false;
		return m->finddata.cFileName;
	}
	return {};
}

std::string DirIterator::fileName() const
{
	return m->finddata.cFileName;
}

std::string DirIterator::filePath() const
{
	return m->path / m->finddata.cFileName;
}

FileInfo DirIterator::fileInfo() const
{
	return FileInfo(next());
}

#else
#include <dirent.h>
#include <cstring>

struct DirIterator::Private {
	std::string path;
	int filter = Dir::Dirs | Dir::Files;
	bool hasnext = false;
	DIR *dir = nullptr;
	dirent *ent = nullptr;
};

DirIterator::DirIterator(const std::string &path, int filter)
	: m(new Private)
{
	m->path = path;
	m->filter = filter;
}

DirIterator::~DirIterator()
{
	if (m->dir) {
		closedir(m->dir);
	}
}

bool DirIterator::hasNext()
{
	if (m->hasnext) return true;
	if (!m->dir) {
		m->dir = opendir(m->path.c_str());
		if (!m->dir) return false;
	}
	while (bool(m->ent = readdir(m->dir))) {
		if (strcmp(m->ent->d_name, ".") == 0) continue;
		if (strcmp(m->ent->d_name, "..") == 0) continue;
		if (m->ent->d_type == DT_DIR) {
			if (m->filter & Dir::Dirs) {
				m->hasnext = true;
				break;
			}
		} else {
			if (m->filter & Dir::Files) {
				m->hasnext = true;
				break;
			}
		}
	}
	return m->hasnext;
}

std::string DirIterator::fileName() const
{
	if (!m->ent) return {};
	return m->ent->d_name;
}

std::string DirIterator::filePath() const
{
	if (!m->ent) return {};
	return m->path / m->ent->d_name;
}

std::string DirIterator::next() const
{
	if (!m->ent) return {};
	if (m->hasnext) {
		m->hasnext = false;
		return m->ent->d_name;
	}
	return {};
}

FileInfo DirIterator::fileInfo() const
{
	if (!m->ent) return {};
	return FileInfo(m->ent->d_name);
}

#endif



