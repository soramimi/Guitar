#include "filetype.h"

#include <cstdio>
#include <fcntl.h>
#include <sys/stat.h>
#include <string>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>


magic_t filetype_open()
{
	char const *mgcfile = "./magic.mgc";

	magic_t magic_cookie = magic_open(MAGIC_MIME);

	if (magic_cookie == NULL) {
		printf("unable to initialize magic library\n");
		return nullptr;
	}

	if (magic_load(magic_cookie, mgcfile) != 0) {
		printf("cannot load magic database - %s\n", magic_error(magic_cookie));
		magic_close(magic_cookie);
		return nullptr;
	}

	return magic_cookie;
}

void filetype_close(magic_t magic_cookie)
{
	magic_close(magic_cookie);
}

std::string filetype_mime_by_data(magic_t magic_cookie, char const *bin, int len)
{
	char tmp[65536];
	len = std::min(len, (int)sizeof(tmp));
	return magic_buffer(magic_cookie, bin, len);
}

#else

#include <magic.h>

bool FileType::open()
{
	magic_cookie = magic_open(MAGIC_MIME);

	if (magic_cookie == nullptr) {
		fprintf(stderr, "unable to initialize magic library\n");
		return false;
	}

	if (magic_load(magic_cookie, nullptr) != 0) {
		fprintf(stderr, "cannot load magic database - %s\n", magic_error(magic_cookie));
		magic_close(magic_cookie);
		return false;
	}

	return true;
}

void FileType::close()
{
	magic_close(magic_cookie);
}

std::string FileType::mime_by_data(const char *bin, int len)
{
	std::string s = magic_buffer(magic_cookie, bin, len);
	auto i = s.find(';');
	if (i != std::string::npos) {
		s = s.substr(0, i);
	}
	return s;
}

#endif
