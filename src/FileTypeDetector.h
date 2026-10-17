#ifndef FILETYPEDETECTOR_H
#define FILETYPEDETECTOR_H

#include <string>
#include <cstdint>
// #include "../subprojects/FileTypePlugin/src/FileTypeWrapper.h"
#include "../subprojects/FileTypePlugin/src/FileType.h"

class FileTypeDetector {
private:
	// FileType filetype;
	FileType::Result detect(const char *data, int64_t size) const;
public:
	FileType::Result detect_by_data(const char *data, int64_t size) const;
	FileType::Result detect_by_file(const char *path) const;
};

#endif // FILETYPEDETECTOR_H
