#include "filetype.h"

//#include <cstdio>
//#include <fcntl.h>
//#include <sys/stat.h>
//#include <QFile>
//#include <string>

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

#include "filetype/file/src/magic.h"

#include <QDebug>
#include <QFile>

bool FileType::open()
{
	magic_cookie = magic_open(MAGIC_MIME);

	if (magic_cookie == nullptr) {
		fprintf(stderr, "unable to initialize magic library\n");
		return false;
	}

#if 0
#ifdef __APPLE__
	char const *mgcfile = "/usr/share/file/magic.mgc";
#else
	char const *mgcfile = "/home/soramimi/develop/filetype/docker/magic.mgc" ;//nullptr;
#endif

	if (magic_load(magic_cookie, mgcfile) != 0) {
		fprintf(stderr, "cannot load magic database - %s\n", magic_error(magic_cookie));
		magic_close(magic_cookie);
		return false;
	}
#else
	QFile file(":/filemagic/magic.mgc");
	file.open(QFile::ReadOnly);
	mgcdata_ = file.readAll();
	void *bufs[1];
	size_t sizes[1];
	bufs[0] = mgcdata_.data();
	sizes[0] = mgcdata_.size();
	auto r = magic_load_buffers(magic_cookie, bufs, sizes, 1);
	Q_ASSERT(r == 0);
#endif

	return true;
}

void FileType::close()
{
	if (magic_cookie) {
		magic_close(magic_cookie);
		magic_cookie = nullptr;
	}
}

std::string FileType::mime_by_data(const char *bin, int len)
{
	auto *p = magic_buffer(magic_cookie, bin, len);
	std::string s;
	if (p) s = p;
	auto i = s.find(';');
	if (i != std::string::npos) {
		s = s.substr(0, i);
	}
	return s;
}

#endif
