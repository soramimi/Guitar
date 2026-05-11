#ifndef REALPATH_H
#define REALPATH_H

#include <string>

namespace misc {

std::string realpath(const char *path);
std::string realpath(std::string const &path);

}

#endif // REALPATH_H
