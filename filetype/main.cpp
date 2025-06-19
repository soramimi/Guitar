
#include "src/FileType.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#ifndef _WIN32
#define O_BINARY 0
#endif

char const *path = "C:\\develop\\Guitar\\filetype\\_bin\\filetype-example.exe";
FileType ft;

void test1()
{
	int fd = open(path, O_RDONLY | O_BINARY);
	if (fd != -1) {
		FileType::Result r = ft.file(fd);
		puts(r.mimetype.c_str());
		close(fd);
	}
}

void test2()
{
	int fd = open(path, O_RDONLY | O_BINARY);
	if (fd != -1) {
		struct stat st;
		if (fstat(fd, &st) == 0 && st.st_size > 0) {
			ssize_t nbytes = std::min(4096, (int)st.st_size);
			std::vector<char> buf(nbytes);
			nbytes = read(fd, buf.data(), buf.size());
			if (nbytes > 0) {
				FileType::Result r = ft.file(buf.data(), buf.size(), st.st_mode);
				puts(r.mimetype.c_str());
			}
		}
		close(fd);
	}
}

void test3()
{
	FileType::Result r = ft.file(path);
	puts(r.mimetype.c_str());
}

int main(int argc, char **argv)
{
	bool f = ft.open("C:\\develop\\Guitar\\filetype\\lib\\magic.mgc");
	test1();
	test2();
	test3();
	return 0;
}
