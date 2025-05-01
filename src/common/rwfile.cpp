
#include "rwfile.h"
#include <optional>
#include <vector>

#ifdef _WIN32
#pragma warning(disable:4996)
#include <io.h>
#else
#include <unistd.h>
#define O_BINARY 0
#endif

#include <fcntl.h>
#include <sys/stat.h>

std::optional<std::vector<char>> readfile(char const *path, int maxsize)
{
	bool ok = false;
	std::vector<char> out;
#ifdef _WIN32
	int fd = open(path, O_RDONLY | O_BINARY);
#else
	int fd = open(path, O_RDONLY);
#endif
	if (fd != -1) {
		struct stat st;
		if (fstat(fd, &st) == 0) {
			if (maxsize < 0 || st.st_size <= maxsize) {
				ok = true;
				out.resize(st.st_size);
				size_t pos = 0;
				while (pos < st.st_size) {
					int n = st.st_size - pos;
					if (n > 65536) {
						n = 65536;
					}
					if (read(fd, &out.at(pos), n) != n) {
						out.clear();
						ok = false;
						break;
					}
					pos += n;
				}
			}
		}
		close(fd);
	}
	if (ok) return out;
	return std::nullopt;
}

bool writefile(char const *path, char const *ptr, size_t len)
{
	bool ok = false;
#ifdef _WIN32
	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, S_IREAD | S_IWRITE);
#else
	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
#endif
	if (fd != -1) {
		ok = true;
		size_t pos = 0;
		while (pos < len) {
			size_t n = len - pos;
			if (n > 65536) {
				n = 65536;
			}
			if (write(fd, ptr + pos, n) != n) {
				ok = false;
				break;
			}
			pos += n;
		}
		close(fd);
	}
	return ok;
}


