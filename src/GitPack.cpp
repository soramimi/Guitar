#include "GitPack.h"
#include "../myzlib.h"
#include <QDebug>
#include <QFile>
#include "GitPackIdxV2.h"

void GitPack::decodeTree(QByteArray *out)
{
	if (out && out->size() > 0) {
		QByteArray ba;
		uint8_t const *begin = (uint8_t const *)out->data();
		uint8_t const *end = begin + out->size();
		uint8_t const *ptr = begin;
		while (ptr < end) {
			int mode = 0;
			while (ptr < end) {
				int c = *ptr & 0xff;
				ptr++;
				if (isdigit(c & 0xff)) {
					mode = mode * 10 + (c - '0');
				} else if (c == ' ') {
					break;
				}
			}
			uint8_t const *left = ptr;
			while (ptr < end && *ptr) {
				ptr++;
			}
			std::string name(left, ptr);
			if (ptr + 20 < end) {
				ptr++;
				char tmp[100];
				sprintf(tmp, "%06u %s ", mode, mode < 100000 ? "tree" : "blob");
				char *p = tmp + 12;
				for (int i = 0; i < 20; i++) {
					sprintf(p, "%02x", ptr[i]);
					p += 2;
				}
				ba.append(tmp, p - tmp);
				ba.append('\t');
				ba.append(name.c_str(), name.size());
				ba.append('\n');
				ptr += 20;
			} else {
				break;
			}
		}
		*out = std::move(ba);
	}

}

Git::Object::Type GitPack::stripHeader(QByteArray *out)
{
	if (out) {
		int n = out->size();
		if (n > 0) {
			char const *p = out->data();
			if (n > 16) n = 16;
			for (int i = 0; i < n; i++) {
				if (p[i] == 0) {
					Git::Object::Type type = Git::Object::Type::UNKNOWN;
					if (strncmp(p, "blob ", 5) == 0) {
						type = Git::Object::Type::BLOB;
					} else if (strncmp(p, "tree ", 5) == 0) {
						type = Git::Object::Type::TREE;
					} else if (strncmp(p, "commit ", 7) == 0) {
						type = Git::Object::Type::COMMIT;
					} else if (strncmp(p, "tag ", 4) == 0) {
						type = Git::Object::Type::TAG;
					}
					if (type != Git::Object::Type::UNKNOWN) {
						*out = out->mid(i + 1);
					}
					return type;
				}
			}
		}
	}
	return Git::Object::Type::UNKNOWN;
}

bool GitPack::decompress(QIODevice *in, size_t expanded_size, QByteArray *out, size_t *consumed, uint32_t *crc)
{
	if (consumed) *consumed = 0;
	try {
		int err;
		z_stream d_stream;

		d_stream.zalloc = nullptr;
		d_stream.zfree = nullptr;
		d_stream.opaque = nullptr;

		d_stream.next_in  = nullptr;
		d_stream.avail_in = 0;

		err = inflateInit(&d_stream);

		if (err != Z_OK) {
			throw QString("failed: inflateInit");
		}

		while (1) {
			if (expanded_size > 0 && (size_t)out->size() > expanded_size) {
				throw QString("file too large");
			}
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

			int in_len = (uint8_t *)d_stream.next_in - src;
			if (consumed) *consumed += in_len;

			if (crc) *crc = crc32(*crc, src, in_len);

			int out_len = d_stream.total_out - total;
			out->append((char const *)tmp, out_len);

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

bool GitPack::seekPackedObject(QIODevice *file, const GitPackIdxItem *item, Info *out)
{
	try {
		Info info;

		auto Read = [&](void *ptr, size_t len){
			const auto l = file->read((char *)ptr, len);
			if (l < 0 || ((size_t)(l)) != len) {
				throw QString("failed to read");
			}
			info.checksum = crc32(info.checksum, (uint8_t const *)ptr, len);
		};

		file->seek(0);

		uint32_t header[3];
		Read(header, sizeof(int32_t) * 3);
		if (memcmp(header, "PACK", 4) != 0) throw QString("invalid pack file");
		uint32_t version = read_uint32_be(header + 1);
		if (version < 2) throw QString("invalid pack file version");
		/*int count = */read_uint32_be(header + 2);

		file->seek(item->offset);

		info.checksum = 0;

		// cf. https://github.com/github/git-msysgit/blob/master/builtin/unpack-objects.c
		{
			size_t size = 0;
			char c;
			Read(&c, 1);
			info.type = (Git::Object::Type)((c >> 4) & 7);
			size = c & 0x0f;
			int shift = 4;
			while (c & 0x80) {
				Read(&c, 1);
				size |= (c & 0x7f) << shift;
				shift += 7;
			}
			info.expanded_size = size;
		}
		if (info.type == Git::Object::Type::OFS_DELTA) {
			uint64_t offset = 0;
			char c;
			Read(&c, 1);
			offset = c & 0x7f;
			while (c & 0x80) {
				Read(&c, 1);
				offset = ((offset + 1) << 7) | (c & 0x7f);
			}
			info.offset = offset;
		} else if (info.type == Git::Object::Type::REF_DELTA) {
			char bin[20];
			Read(bin, 20);
			char tmp[41];
			for (int i = 0; i < 20; i++) {
				sprintf(tmp + i * 2, "%02x", bin[i] & 0xff);
			}
			info.ref_id = QString::fromLatin1(tmp, GIT_ID_LENGTH);
		}

		*out = info;
		return true;
	} catch (QString const &e) {
		qDebug() << e;
	}
	return false;
}

bool GitPack::load(QIODevice *file, const GitPackIdxItem *item, Object *out)
{
	*out = Object();
	try {
		seekPackedObject(file, item, out);

		if (decompress(file, out->expanded_size, &out->content, &out->packed_size, &out->checksum)) {
			out->expanded_size = out->expanded_size;
			return true;
		}
	} catch (QString const &e) {
		qDebug() << e;
	}
	return false;
}

bool GitPack::load(QString const &packfile, const GitPackIdxItem *item, GitPack::Object *out)
{
	QFile file(packfile);
	if (file.open(QFile::ReadOnly)) {
		if (load(&file, item, out)) {
			return true;
		}
	}
	return false;
}

