#ifndef FILETYPE_H
#define FILETYPE_H

#include <QObject>
#include "FileTypeWrapper.h"

class FileType : public QObject {
	Q_OBJECT
private:
	struct Private;
	Private *m;
public:
	FileType();
	~FileType();
	
	using Result = FileTypeWrapper::Result;
	
	virtual bool open();
	virtual Result detect(char const *filepath) const;
	virtual Result detect(const char *data, size_t size, int filemode = 0644) const;
};

#endif // FILETYPE_H
