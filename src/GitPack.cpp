#include "GitPack.h"
#include "../zlib.h"
#include <QDebug>
#include <QFile>

bool GitPack::decompress(QIODevice *in, bool process_header, Type type, size_t expanded_size, QByteArray *out, size_t *consumed)
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

		char header_buf[100] = {0};
		int header_pos = process_header ? 0 : -1;

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
			int n;

			n = d_stream.avail_in;
			err = ::inflate(&d_stream, Z_NO_FLUSH);
			n -= d_stream.avail_in;
			if (consumed) *consumed += n;

			n = d_stream.total_out - total;
			char const *p = (char const *)tmp;
			while (header_pos >= 0 && n > 0) {
				char c = *p;
				header_buf[header_pos] = c;
				if (header_pos + 1 < sizeof(header_buf)) {
					header_pos++;
				}
				p++;
				n--;
				if (c == 0) {
					header_pos = -1;
					break;
				}
			}
			out->append(p, n);
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

		if (process_header) {
			if (strncmp(header_buf, "tree ", 5) == 0) {
				type = Type::TREE;
			}
		}

		int n = out->size();
		if (n > 0 && type == Type::TREE) {
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

		if (decompress(file, false, out->type, size, &out->content, &out->packed_size)) {
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

