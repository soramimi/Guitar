#include "gzip.h"
#include <zlib.h>
#include <cstdint>
#include <string.h>

void gzip::set_header_only(bool f)
{
	header_only_ = f;
}

void gzip::set_maximul_size(int64_t size)
{
	max_size_ = size;
}

bool gzip::decompress(AbstractSimpleReader *input, AbstractSimpleWriter *output)
{
	error_ = {};

	auto Do = [&](){
		struct Header {
			uint8_t id1;
			uint8_t id2;
			uint8_t cm;
			uint8_t flg;
			uint8_t mtime[4];
			uint8_t xfl;
			uint8_t os;
		};
		enum {
			FTEXT    = 0x01,
			FHCRC    = 0x02,
			FEXTRA   = 0x04,
			FNAME    = 0x08,
			FCOMMENT = 0x10,
		};

		unsigned char ibuf[65536];
		int n;

		n = input->read((char *)ibuf, sizeof(Header));
		if (n != 10) {
			error_ = "failed to read the header";
			return false;
		}

		auto *h = (Header *)ibuf;

		if (h->id1 == 0x1f && h->id2 == 0x8b && h->cm == 8) {
			// ok
			if (header_only_) {
				return true;
			}
		} else {
			error_ = "not a gzip file";
			return false;
		}

		auto ReadText = [&](){
			char c;
			std::vector<uint8_t> vec;
			while (1) {
				if (input->read(&c, 1) != 1) {
					break;
				}
				if (c == 0) break;
				vec.push_back((uint8_t)c);
			}
			std::string str;
			if (!vec.empty()) {
				str.assign((char const *)&vec[0], vec.size());
			}
			return str;
		};

		if (h->flg & FEXTRA) {
			input->read((char *)ibuf, 2);
			n = ((uint8_t)ibuf[1] << 8) | (uint8_t)ibuf[0];
			input->seek(input->pos() + n);
		}
		if (h->flg & FNAME) {
			std::string name = ReadText();
			(void)name;
		}
		if (h->flg & FCOMMENT) {
			std::string comment = ReadText();
			(void)comment;
		}
		if (h->flg & FHCRC) {
			input->read((char *)ibuf, 2);
		}

		int64_t inpos = input->pos();

		z_stream stream = {};
		uint32_t crc = 0;
		int err;

		stream.zalloc = nullptr;
		stream.zfree = nullptr;
		stream.opaque = nullptr;

		stream.next_in = ibuf;
		stream.avail_in = 0;

		err = inflateInit2(&stream, -MAX_WBITS);

		if (err != Z_OK) {
			error_ = "inflateInit2 failed";
			return false;
		}

		{
			bool ok = false;
			while (1) {
				if (stream.avail_in == 0) {
					stream.next_in = ibuf;
					unsigned char *p = ibuf + stream.avail_in;
					auto len = input->read((char *)p, sizeof(ibuf) - stream.avail_in);
					stream.avail_in += len;
				}
				unsigned char obuf[65536];
				stream.next_out = obuf;            /* discard the output */
				stream.avail_out = sizeof(obuf);
				if (max_size_ != -1 && stream.total_out + stream.avail_out > (unsigned)max_size_ && (unsigned)max_size_ >= stream.total_out) {
					stream.avail_out = size_t(max_size_ - stream.total_out);
				}
				uLong total_out = stream.total_out;
				err = ::inflate(&stream, Z_NO_FLUSH);
				int n = int(stream.total_out - total_out);

				if (output) {
					int w = (int)output->write((char const *)obuf, n);
					if (w != n) {
						error_ = "failed to write to the output device";
						return false;
					}
				}

				crc = crc32(crc, (unsigned char const *)obuf, (size_t)n);
				if (err == Z_STREAM_END) {
					ok = true;
					break;
				}
				if (err != Z_OK) {
					error_ = "inflate failed: " + std::string(zError(err));
					return false;
				}
				if (stream.total_out >= (unsigned)max_size_) {
					break;
				}
			}
			inflateEnd(&stream);
			if (!ok) return false;
		}

		input->seek(inpos + stream.total_in);
		input->read((char *)ibuf, 8);

		auto ReadU32LE = [](void const *p)->uint32_t{
			auto const *q = (uint8_t const *)p;
			return (q[3] << 24) | (q[2] << 16) | (q[1] << 8) | q[0];
		};

		auto c = ReadU32LE(ibuf);
		auto l = ReadU32LE(ibuf + 4);
		if (c != crc) {
			error_ = "crc incorrect";
			return false;
		}
		if (l != stream.total_out) {
			error_ = "length incorrect";
			return false;
		}

		return true;
	};

	return Do();
}

bool gzip::is_valid_gz_file(AbstractSimpleReader *input)
{
	gzip z;
	z.set_header_only(true);
	return z.decompress(input, nullptr);
}

