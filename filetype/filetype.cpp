
#include "filetype.h"
#include <magic.h>
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
