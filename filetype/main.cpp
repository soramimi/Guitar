#include <stdio.h>
#include <magic.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <memory>
#include <algorithm>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#define O_BINARY 0
#endif

#include "magic.h"

int main(void)
{
	char const *mgcfile = "../misc/magic.mgc";
#ifdef _WIN32
	char const *actual_file = "C:/develop/Guitar/src/AddRepositoryDialog.cpp";

#else
	char const *actual_file = "filetype";
#endif

	const char *magic_full;
	magic_t magic_cookie;

	magic_cookie = magic_open(MAGIC_MIME);

	if (magic_cookie == NULL) {
		printf("unable to initialize magic library\n");
		return 1;
	}

#if 0
	if (magic_load(magic_cookie, mgcfile) != 0) {
		printf("cannot load magic database - %s\n", magic_error(magic_cookie));
		magic_close(magic_cookie);
		return 1;
	}
#else
	std::unique_ptr<char[]> mgcdata;
	{
		int fd = open(mgcfile, O_RDONLY);
		if (fd != -1) {
			struct stat st;
			if (fstat(fd, &st) == 0 && st.st_size > 0) {
				mgcdata.reset(new char[st.st_size]);
				read(fd, mgcdata.get(), st.st_size);
				void *bufs[1];
				size_t sizes[1];
				bufs[0] = mgcdata.get();
				sizes[0] = st.st_size;
				magic_load_buffers(magic_cookie, bufs, sizes, 1);
			}
			close(fd);
		}
	}
#endif

#if 0
	magic_full = magic_file(magic_cookie, actual_file);
#else
	magic_full = NULL;
	char tmp[65536];
	int fd = open(actual_file, O_RDONLY | O_BINARY);
	if (fd != -1) {
		int n = read(fd, tmp, sizeof(tmp));
		if (n > 0) {
			n = std::min(n, (int)sizeof(tmp));
			magic_full = magic_buffer(magic_cookie, tmp, n);
		}
	}
#endif

	printf("%s\n", magic_full ? magic_full : "");
	magic_close(magic_cookie);
	return 0;
}
