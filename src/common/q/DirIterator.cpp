#include "DirIterator.h"
#include "common/misc.h"
#include "common/joinpath.h"
#include <filesystem>

#ifdef _WIN32
#include <Windows.h>

struct DirIterator::Private {
	std::string path;
	int filter = Dir::Dirs | Dir::Files;
	bool hasnext = false;
	WIN32_FIND_DATAW finddata = {};
	HANDLE handle = nullptr;
};

DirIterator::DirIterator(std::string const &path, int filter)
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
		std::filesystem::path filter = misc::convert_utf8_to_utf16(m->path / "*.*");
		filter.make_preferred();
		m->handle = FindFirstFileW(filter.c_str(), &m->finddata);
		if (m->handle == INVALID_HANDLE_VALUE) {
			m->handle = nullptr;
			return false;
		}
	} else {
		if (!FindNextFileW(m->handle, &m->finddata)) {
			return false;
		}
	}
	do {
		if (wcscmp(m->finddata.cFileName, L".") == 0) continue;
		if (wcscmp(m->finddata.cFileName, L"..") == 0) continue;
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
	} while (FindNextFileW(m->handle, &m->finddata));
	return m->hasnext;
}

std::string DirIterator::next() const
{
	if (m->hasnext) {
		m->hasnext = false;
		// return m->finddata.cFileName;
		return fileName();
	}
	return {};
}

std::string DirIterator::fileName() const
{
	std::filesystem::path filename = m->finddata.cFileName;
	return filename.string();
}

std::string DirIterator::filePath() const
{
	std::filesystem::path path = misc::convert_utf8_to_utf16(m->path);
	path = path / m->finddata.cFileName;
	path.make_preferred();
	return misc::convert_utf16_to_utf8(path.u16string());
}

FileInfo DirIterator::fileInfo() const
{
	return FileInfo(next());
}

#else
#include <dirent.h>
#include <cstring>

struct DirIterator::Private {
	std::filesystem::path path;
	int filter = Dir::Dirs | Dir::Files;
	bool hasnext = false;
	DIR *dir = nullptr;
	dirent *ent = nullptr;
};

DirIterator::DirIterator(std::string const &path, int filter)
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



