#include "FileType.h"

struct FileType::Private {
	FileTypeWrapper detector;
};

FileType::FileType()
	: m(new Private)
{
}

FileType::~FileType()
{
	delete m;
}

bool FileType::open()
{
	return true;
}

FileType::Result FileType::detect(const char *data, size_t size, int filemode) const
{
	return m->detector.detect(data, size, filemode);
}

FileType::Result FileType::detect(const char *filepath) const
{
	return m->detector.detect(filepath);
}

