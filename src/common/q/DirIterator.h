#ifndef DIRITERATOR_H
#define DIRITERATOR_H

#include "FileInfo.h"
#include "Dir.h"

#ifdef _WIN32
class DirIterator {
private:
	struct Private;
	Private *m;
public:
	DirIterator(std::string const &path, int filter = Dir::Dirs | Dir::Files);
	~DirIterator();
	bool hasNext();
	std::string fileName() const;
	std::string filePath() const;
	std::string next() const;
	FileInfo fileInfo() const;
};
#else
class DirIterator {
private:
	struct Private;
	Private *m;
public:
	DirIterator(std::string const &path, int filter = Dir::Dirs | Dir::Files);
	~DirIterator();
	bool hasNext();
	std::string fileName() const;
	std::string filePath() const;
	std::string next() const;
	FileInfo fileInfo() const;
};
#endif

#endif // DIRITERATOR_H
