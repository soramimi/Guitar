#ifndef FILETYPEINTERFACE_H
#define FILETYPEINTERFACE_H

#include <QtPlugin>
#include "FileType.h"

class FileTypeInterface {
public:
	FileTypeInterface();
	virtual ~FileTypeInterface() {}
	virtual FileType *create() = 0;
};

Q_DECLARE_INTERFACE(FileTypeInterface, "mynamespace.FileTypePlugin/1.0")

#endif // FILETYPEINTERFACE_H
