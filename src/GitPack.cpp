#include "GitPack.h"
#include <QtZlib/zlib.h>
#include <QDebug>
#include <QFile>

size_t GitPack::decompress(QIODevice *in, size_t expanded_size, QByteArray *out, size_t *consumed)
{
	if (consumed) *consumed = 0;
	try {
		int err;
		z_stream d_stream;

		d_stream.zalloc = (alloc_func)0;
		d_stream.zfree = (free_func)0;
		d_stream.opaque = (voidpf)0;

		d_stream.next_in  = 0;
		d_stream.avail_in = 0;

		err = inflateInit(&d_stream);

		if (err != Z_OK) {
			throw QString("failed: inflateInit");
		}

		while ((size_t)out->size() < expanded_size) {
			uint8_t src[1024];
			uint8_t tmp[65536];
			if (d_stream.next_in != src && d_stream.avail_in > 0) {
				memmove(src, d_stream.next_in, d_stream.avail_in);
			}
			d_stream.next_in = src;
			if (d_stream.avail_in < sizeof(src)) {
				int n = sizeof(src) - d_stream.avail_in;
				n = in->read((char *)(src + d_stream.avail_in), n);
				if (n >= 0) {
					d_stream.avail_in += n;
				}
			}

			d_stream.next_out = tmp;
			size_t l = expanded_size - out->size();
			if (l > sizeof(tmp)) l = sizeof(tmp);
			d_stream.avail_out = l;
			uLong total = d_stream.total_out;
			int n;

			n = d_stream.avail_in;
			err = ::inflate(&d_stream, Z_NO_FLUSH);
			n -= d_stream.avail_in;
			if (consumed) *consumed += n;

			n = d_stream.total_out - total;
			out->append((char const *)tmp, n);
			if (err == Z_STREAM_END) {
				break;
			}
			if (err != Z_OK) {
				throw QString("failed: inflate");
			}
		}

		err = inflateEnd(&d_stream);
		if (err != Z_OK) {
			throw QString("failed: inflateEnd");
		}

		return true;
	} catch (QString const &e) {
		qDebug() << e;
	}
	return false;
}

bool GitPack::load(QIODevice *file, const GitPackIdxV2::Item *item, GitPack::Object *out)
{
	*out = Object();
	try {
		auto Read = [&](void *ptr, size_t len){
			return file->read((char *)ptr, len) == len;
		};
		uint32_t header[3];
		if (!Read(header, sizeof(int32_t) * 3)) throw QString("failed to read the header");
		if (memcmp(header, "PACK", 4) != 0) throw QString("invalid pack file");
		uint32_t version = read_uint32_be(header + 1);
		if (version < 2) throw "invalid pack file version";
		/*int count = */read_uint32_be(header + 2);

		file->seek(item->offset);

		uint64_t size = 0;
		{
			int shift = 0;
			while (1) {
				char c;
				if (!Read(&c, 1)) throw QString("failed to read");
				if (shift == 0) {
					out->type = (GitPack::Type)((c >> 4) & 7);
					size = c & 0x0f;
					shift = 4;
				} else {
					size |= (c & 0x7f) << shift;
					shift += 7;
				}
				if (!(c & 0x80)) break;
			}
		}
		if (out->type == GitPack::Type::OFS_DELTA) {
			uint64_t offset = 0;
			int shift = 0;
			while (1) {
				char c;
				if (!Read(&c, 1)) throw QString("failed to read");
				offset |= (c & 0x7f) << shift;
				shift += 7;
				if (!(c & 0x80)) break;
			}
			out->offset = offset;
		} else if (out->type == GitPack::Type::REF_DELTA) {
			char tmp[20];
			if (!Read(tmp, 20)) throw QString("failed to read");
		}

		if (decompress(file, size, &out->content, &out->packed_size)) {
			out->expanded_size = size;
			return true;
		}
	} catch (QString const &e) {
		qDebug() << e;
	}
	return false;
}

bool GitPack::load(QString const &packfile, const GitPackIdxV2::Item *item, GitPack::Object *out)
{
	QFile file(packfile);
	if (file.open(QFile::ReadOnly)) {
		if (load(&file, item, out)) {
			return true;
		}
	}
	return false;
}

