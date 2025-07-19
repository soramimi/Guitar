#ifndef RWFILE_H
#define RWFILE_H

#include <vector>
#include <cstddef>  // for size_t
#include <stdlib.h>
#include <optional>

std::optional<std::vector<char>> readfile(char const *path, int maxsize);
bool writefile(char const *path, char const *ptr, size_t len);

#endif // RWFILE_H
