
#include "rwfile.h"
#ifdef WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#include <fcntl.h>
#include <sys/stat.h>

#ifdef WIN32
#pragma warning(disable:4996)
#endif

bool readfile(char const *path, std::vector<char> *out, int maxsize)
{
	out->clear();
	bool ok = false;
#ifdef WIN32
	int fd = open(path, O_RDONLY | O_BINARY);
#else
	int fd = open(path, O_RDONLY);
#endif
	if (fd != -1) {
		struct stat st;
		if (fstat(fd, &st) == 0) {
			if (maxsize < 0 || st.st_size <= maxsize) {
				ok = true;
				out->resize(st.st_size);
				int pos = 0;
				while (pos < st.st_size) {
					int n = st.st_size - pos;
					if (n > 65536) {
						n = 65536;
					}
					if (read(fd, &out->at(pos), n) != n) {
						out->clear();
						ok = false;
						break;
					}
					pos += n;
				}
			}
		}
		close(fd);
	}
	return ok;
}

bool writefile(char const *path, std::vector<char> const *vec)
{
	bool ok = false;
#ifdef WIN32
	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, S_IREAD | S_IWRITE);
#else
	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
#endif
	if (fd != -1) {
		ok = true;
		int pos = 0;
		while (pos < (int)vec->size()) {
			int n = (int)vec->size() - pos;
			if (n > 65536) {
				n = 65536;
			}
			if (write(fd, &vec->at(pos), n) != n) {
				ok = false;
				break;
			}
			pos += n;
		}
		close(fd);
	}
	return ok;
}


