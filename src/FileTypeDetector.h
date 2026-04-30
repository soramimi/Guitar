#ifndef FILETYPEDETECTOR_H
#define FILETYPEDETECTOR_H

#include <string>
#include <cstdint>
#include "filetype/src/FileType.h"

class FileTypeDetector {
private:
	FileType filetype;

	bool open()
	{
		return filetype.open();
	}
	void close()
	{
		filetype.close();
	}
	FileType::Result detect(const char *data, int64_t size) const
	{
		return filetype.detect(data, size);
	}
public:
	FileTypeDetector()
	{
		open();
	}
	~FileTypeDetector()
	{
		close();
	}
	std::string mimetype_by_data(const char *data, int64_t size) const;
	std::string mimetype_by_file(const char *path) const;
	std::string mimetype_by_file(std::string const &path) const
	{
		return mimetype_by_file(path.c_str());
	}
};

#endif // FILETYPEDETECTOR_H
