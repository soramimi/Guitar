
#include "FileType.h"
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

	QFile file(":/filemagic/magic.mgc"); // load magic from resource
	file.open(QFile::ReadOnly);
	mgcdata_ = file.readAll();
	void *bufs[1];
	size_t sizes[1];
	bufs[0] = mgcdata_.data();
	sizes[0] = mgcdata_.size();
	auto r = magic_load_buffers(magic_cookie, bufs, sizes, 1);
	return r == 0;
}

void FileType::close()
{
	if (magic_cookie) {
		magic_close(magic_cookie);
		magic_cookie = nullptr;
	}
}

QString FileType::mime_by_data(const char *bin, int len)
{
	Q_ASSERT(magic_cookie); // not called open() yet or failed to load magic file

	if (!bin || len < 1) return {};
	auto *p = magic_buffer(magic_cookie, bin, len);
	if (!p) return {};
	QString s = p;
	auto i = s.indexOf(';');
	if (i >= 0) {
		s = s.left(i);
	}
	return s.trimmed();
}

QString FileType::mime_by_file(QString const &path)
{
	QFile file(path);
	if (file.open(QFile::ReadOnly)) {
		QByteArray ba = file.readAll();
		return mime_by_data(ba.data(), ba.size());
	}
	return {};
}
