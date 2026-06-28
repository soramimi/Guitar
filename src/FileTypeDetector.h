#ifndef FILETYPEDETECTOR_H
#define FILETYPEDETECTOR_H

#include <string>
#include <cstdint>
#include "FileTypeWrapper.h"

class FileTypeDetector {
private:
	FileTypeWrapper filetype;
	using Result = FileTypeWrapper::Result;

	Result detect(const char *data, int64_t size) const;
public:
	std::string mimetype_by_data(const char *data, int64_t size) const;
	std::string mimetype_by_file(const char *path) const;
	std::string mimetype_by_file(std::string const &path) const
	{
		return mimetype_by_file(path.c_str());
	}
};

#endif // FILETYPEDETECTOR_H
