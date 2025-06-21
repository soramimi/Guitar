#ifndef RWFILE_H
#define RWFILE_H

#include <vector>
#include <stdlib.h>

bool readfile(char const *path, std::vector<char> *out, int maxsize);
bool writefile(char const *path, char const *ptr, size_t len);

#endif // RWFILE_H
