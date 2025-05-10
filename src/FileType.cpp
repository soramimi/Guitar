
#include "FileType.h"
#include "MemoryReader.h"
#include "gunzip.h"
#include <magic.h>
#include <QDebug>
#include <QFile>
#include <QBuffer>
#include <string>
#include "common/misc.h"

FileType::FileType()
{
	//open(); // このインスタンスを作成した側でopen()を呼ぶこと
}

FileType::~FileType()
{
	close();
}

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

std::string FileType::mime_by_data(const char *bin, int len) const
{
	Q_ASSERT(magic_cookie); // not called open() yet or failed to load magic file

	if (!bin || len < 1) return {};
	auto *p = magic_buffer(magic_cookie, bin, len);
	if (!p) return {};
	std::string s = p;
	auto i = s.find(';');
	if (i != std::string::npos) {
		s = s.substr(0, i);
	}
	return std::string{misc::trimmed(s)};
}

std::string FileType::mime_by_file(QString const &path)
{
	QFile file(path);
	if (file.open(QFile::ReadOnly)) {
		QByteArray ba = file.readAll();
		return mime_by_data(ba.data(), ba.size());
	}
	return {};
}

std::string FileType::determin(const QByteArray &in) const
{
	if (in.isEmpty()) return {};

	if (in.size() > 10) {
		if (memcmp(in.data(), "\x1f\x8b\x08", 3) == 0) { // gzip
			QBuffer buf;
			MemoryReader reader(in.data(), in.size());

			reader.open(MemoryReader::ReadOnly);
			buf.open(QBuffer::WriteOnly);
			gunzip z;
			z.set_maximul_size(100000);
			z.decode(&reader, &buf);

			QByteArray uz = buf.buffer();
			if (!uz.isEmpty()) {
				return mime_by_data(uz.data(), uz.size());
			}
		}
	}

	return mime_by_data(in.data(), in.size());
}

std::string FileType::determin(const QString &path) const
{
	QFile file(path);
	if (file.open(QIODevice::ReadOnly)) {
		QByteArray ba;
		ba = file.read(65536);
		file.close();
		return determin(ba);
	}
	return {};
}
