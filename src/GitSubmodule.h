#ifndef GITSUBMODULE_H
#define GITSUBMODULE_H

#include "GitHash.h"
#include <string>

struct GitSubmoduleItem {
	std::string name;
	GitHash id;
	std::string path;
	std::string refs;
	std::string url;
	explicit operator bool () const
	{
		return id.isValid() && !path.empty();
	}
};

struct GitSubmoduleUpdateData {
	bool init = true;
	bool recursive = true;
};

#endif // GITSUBMODULE_H
