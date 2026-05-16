#ifndef __RWFILE_H
#define __RWFILE_H

#include <vector>

bool readfile(char const *path, std::vector<char> *out, int maxsize = -1);
bool writefile(char const *path, std::vector<char> const *vec);

#endif

