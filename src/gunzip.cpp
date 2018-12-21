#include "gunzip.h"
#include <zlib.h>
#include <stdint.h>
#include <QFile>

void gunzip::set_header_only(bool f)
{
	header_only = f;
}

void gunzip::set_maximul_size(int64_t size)
{
	maxsize = size;
}

bool gunzip::decode(QIODevice *input, QIODevice *output)
{
	error = QString();
	try {
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

		unsigned char ibuf[4096];
		bool ok = true;
		int n;

		n = input->read((char *)ibuf, sizeof(Header));
		if (n != 10) {
			throw QString("failed to read the header");
		}

		auto *h = (Header *)ibuf;

		if (h->id1 == 0x1f && h->id2 == 0x8b && h->cm == 8) {
			// ok
			if (header_only) {
				return true;
			}
		} else {
			throw QString("invalid input format");
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
			n = input->read((char *)ibuf, 2);
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

		z_stream stream;
		uint32_t crc = 0;
		int err;

		stream.zalloc = nullptr;
		stream.zfree = nullptr;
		stream.opaque = nullptr;

		stream.next_in = ibuf;
		stream.avail_in = 0;

		err = inflateInit2(&stream, -MAX_WBITS);

		if (err != Z_OK) {
			throw QString("inflateInit2 faled");
		}

		if (open) {
			if (!open(output)) {
				ok = false;
			}
		} else {
			if (!output->open(QIODevice::WriteOnly)) {
				ok = false;
			}
		}
		if (!ok) throw QString("failed to open the output device");

		int64_t inpos = input->pos();

		auto Close = [&](){
			if (close) {
				close(output);
			} else {
				output->close();
			}
			inflateEnd(&stream);
		};

		try {
			while (1) {
				if (stream.avail_in == 0) {
					stream.next_in = ibuf;
				}
				if (stream.avail_in < sizeof(ibuf)) {
					stream.next_in = ibuf + stream.avail_in;
					auto len = input->read((char *)stream.next_in, sizeof(ibuf) - stream.avail_in);
					stream.avail_in += len;
				}
				unsigned char obuf[65536];
				stream.next_out = obuf;            /* discard the output */
				stream.avail_out = sizeof(obuf);
				if (maxsize != -1 && stream.total_out + stream.avail_out > (unsigned)maxsize && (unsigned)maxsize >= stream.total_out) {
					stream.avail_out = maxsize - stream.total_out;
				}
				uLong total_out = stream.total_out;
				err = ::inflate(&stream, Z_NO_FLUSH);
				int n = stream.total_out - total_out;

				if (write) {
					if (!write(output, (char const *)obuf, n)) {
						ok = false;
					}
				} else {
					int w = output->write((char const *)obuf, n);
					if (w != n) {
						ok = false;
					}
				}
				if (!ok) throw QString("failed to write to the output device");

				crc = crc32(crc, (unsigned char const *)obuf, n);
				if (err == Z_STREAM_END) {
					break;
				}
				if (err != Z_OK) {
					throw QString("inflate failed");
				}
				if (stream.total_out >= (unsigned)maxsize) {
					break;
				}
			}
		} catch (...) {
			Close();
			throw;
		}
		Close();

		input->seek(inpos + stream.total_in);
		n = input->read((char *)ibuf, 8);

		auto ReadU32LE = [](void const *p)->uint32_t{
			auto const *q = (uint8_t const *)p;
			return (q[3] << 24) | (q[2] << 16) | (q[1] << 8) | q[0];
		};

		auto c = ReadU32LE(ibuf);
		auto l = ReadU32LE(ibuf + 4);
		if (c != crc) throw QString("crc incorrect");
		if (l != stream.total_out) throw QString("length incorrect");

		return true;
	} catch (QString const &e) {
		error = e;
	}
	return false;
}

bool gunzip::decode(QString const &inpath, QString const &outpath)
{
	QFile infile(inpath);
	QFile outfile(outpath);
	if (infile.open(QFile::ReadOnly)) {
		if (decode(&infile, &outfile)) {
			return true;
		}
	}
	return false;
}

bool gunzip::is_valid_gz_file(QIODevice *input)
{
	gunzip z;
	z.set_header_only(true);
	return z.decode(input, nullptr);
}

bool gunzip::is_valid_gz_file(QString const &inpath)
{
	QFile infile(inpath);
	if (infile.open(QFile::ReadOnly)) {
		gunzip z;
		if (z.is_valid_gz_file(&infile)) {
			return true;
		}
	}
	return false;
}
