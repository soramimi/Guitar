
#ifndef GITOBJECTDATA_H
#define GITOBJECTDATA_H

#include "MainWindowTypes.h"
#include <QMetaType>

struct GitObjectData {
	std::string id;
	std::string path;
	GitSubmoduleItem submod;
	GitCommitItem submod_commit;
	QString header;
	size_t idiff;
	bool staged = false;

	bool isUntracked() const
	{
		return header.isEmpty();
	}
};

class MainWindowExchangeData {
public:
	MainWindowFileListType files_list_type;
	std::vector<GitObjectData> object_data;
};
Q_DECLARE_METATYPE(MainWindowExchangeData)

#endif // GITOBJECTDATA_H
