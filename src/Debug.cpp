
#include "Debug.h"
#if 0
#include "GitPackIdxV2.h"
#include <QDebug>
#include <QFile>
#include "GitPackIdxV2.h"
#include <zlib.h>
//#include "crc32.h"

uint32_t read_uint32_be(const void *p)
{
	uint8_t const *q = (uint8_t const *)p;
	return (q[0] << 24) | (q[1] << 16) | (q[2] << 8) | q[3];
}


void decompress(QIODevice *in, int expanded_size, QByteArray *out)
{
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

	while (out->size() < expanded_size) {
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
		err = ::inflate(&d_stream, Z_NO_FLUSH);
		int n = d_stream.total_out - total;
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
}
void Debug::doit()
{
	char const *idx_path = "C:/develop/GetIt/.git/objects/pack/pack-da889d867e8acb4d18c95ed6d519c5609e0e78d5.idx";
	GitPackIdxV2 idx;
	{
		QFile file(idx_path);
		if (file.open(QFile::ReadOnly)) {
			idx.parse(&file);
		}
	}
	int n = idx.count();
	for (int i = 0; i < n; i++) {
		GitPackIdxV2::Item const *item = idx.item(i);
		//	qDebug() << idx.offset(0);
		//	qDebug() << idx.offset(1);
		char const *pack_path = "C:/develop/GetIt/.git/objects/pack/pack-da889d867e8acb4d18c95ed6d519c5609e0e78d5.pack";
		QFile file(pack_path);
		if (file.open(QFile::ReadOnly)) {
			try {
				uint32_t header[3];
				auto Read = [&](void *ptr, size_t len){
					return file.read((char *)ptr, len) == len;
				};
				if (!Read(header, sizeof(int32_t) * 3)) throw QString("failed to read the header");
				if (memcmp(header, "PACK", 4) != 0) throw QString("invalid pack file");
				uint32_t version = read_uint32_be(header + 1);
				if (version < 2) throw "invalid pack file version";
				int count = read_uint32_be(header + 2);

				file.seek(item->offset);
				{//for (int i = 0; i < count; i++) {
					enum class Type {
						UNKNOWN = 0,
						COMMIT = 1,
						BLOB = 2,
						TREE = 3,
						TAG = 4,
						OFS_DELTA = 6,
						REF_DELTA = 7,
					} type = Type::UNKNOWN;
					uint64_t size = 0;
					int shift = 0;
					while (1) {
						char c;
						if (!Read(&c, 1)) throw QString("failed to read");
						if (shift == 0) {
							type = (Type)((c >> 4) & 7);
							size = c & 0x0f;
							shift = 4;
						} else {
							size |= (c & 0x7f) << shift;
							shift += 7;
						}
						if (!(c & 0x80)) break;
					}
					switch (type) {
					case Type::UNKNOWN: qDebug() << "UNKNOWN"; break;
					case Type::COMMIT: qDebug() << "COMMIT"; break;
					case Type::BLOB: qDebug() << "BLOB"; break;
					case Type::TREE: qDebug() << "TREE"; break;
					case Type::TAG: qDebug() << "TAG"; break;
					case Type::OFS_DELTA: qDebug() << "OFS_DELTA"; break;
					case Type::REF_DELTA: qDebug() << "REF_DELTA"; break;

					}

					QByteArray vec;
					decompress(&file, size, &vec);

					//				if (!vec.isEmpty()) {
					//					uint32_t checksum = crc32(0, (Bytef const *)vec.data(), vec.size());
					//					qDebug() << item->checksum;
					//					qDebug() << checksum;
					//				}

					qDebug() << i << size << vec.size();
				}

			} catch (QString const &e) {
				qDebug() << e;
			}
		}
	}
}

#endif
