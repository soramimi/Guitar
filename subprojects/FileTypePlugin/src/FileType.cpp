#include "FileType.h"
#include "FileTypeWrapper.h"

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

FileType::Result::Result(InternalFileTypeResult *result)
{
	if (result) {
		isdir = result->isdir;
		mimetype = QString::fromUtf8(result->mimetype);
		charset = QString::fromUtf8(result->charset);
		void (*_free)(void *p) = result->_free;
		if (_free) {
			_free(result->mimetype);
			_free(result->charset);
			_free(result);
		}
	}
}
