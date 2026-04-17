#ifndef FILEINFO_H
#define FILEINFO_H

#include <sys/stat.h>
#include <string>
#include "Dir.h"

class FileInfo {
private:
	struct Private;
	Private *m;
public:
	FileInfo(std::string const &file);
	virtual ~FileInfo();
	bool isFile() const;
	bool isDir() const;
	Dir dir() const;
	std::string fileName() const;
};

#endif // FILEINFO_H
