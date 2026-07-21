
#ifndef GITCLONEDATA_H
#define GITCLONEDATA_H

#include <string>

struct GitCloneData {
	std::string url;
	std::string basedir;
	std::string subdir;
};

#endif // GITCLONEDATA_H
