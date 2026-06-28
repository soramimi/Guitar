#ifndef FILETYPEPLUGIN_H
#define FILETYPEPLUGIN_H

#include <QtCore/qplugin.h>
#include "FileTypeInterface.h"

class FileTypePlugin : public QObject, public FileTypeInterface {
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "mynamespace.FileTypePlugin" FILE "filetypeplugin.json")
	Q_INTERFACES(FileTypeInterface)
public:
	FileType *create() override
	{
		return new FileType();
	}
};

#endif // FILETYPEPLUGIN_H
