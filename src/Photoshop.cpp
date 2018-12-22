#include "Photoshop.h"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

inline uint32_t read_uint32_be(void const *p)
{
	auto const *q = (uint8_t const *)p;
	return (q[0] << 24) | (q[1] << 16) | (q[2] << 8) | q[3];
}

inline uint16_t read_uint16_be(void const *p)
{
	auto const *q = (uint8_t const *)p;
	return (q[0] << 8) | q[1];
}

} // namespace

void photoshop::readThumbnail(QIODevice *in, std::vector<char> *jpeg)
{
	jpeg->clear();

	struct FileHeader {
		char sig[4];
		char ver[2];
		char reserved[6];
		char channels[2];
		char height[4];
		char width[4];
		char depth[2];
		char colormode[2];
	};
	FileHeader fh;
	in->read((char *)&fh, sizeof(FileHeader));

	if (memcmp(fh.sig, "8BPS", 4) == 0) {
		char tmp[4];
		uint32_t len;
		in->read(tmp, 4);
		len = read_uint32_be(tmp);
		in->seek(in->pos() + len);
		in->read(tmp, 4);
		len = read_uint32_be(tmp);

		while (1) {
			struct ImageResourceHeader {
				char sig[4];
				char id[2];
			};
			ImageResourceHeader irh;
			in->read((char *)&irh, sizeof(ImageResourceHeader));
			if (memcmp(irh.sig, "8BIM", 4) == 0) {
				std::vector<char> name;
				while (1) {
					char c;
					if (in->read(&c, 1) != 1) break;
					if (c == 0) {
						if ((name.size() & 1) == 0) {
							if (in->read(&c, 1) != 1) break;
						}
						break;
					}
					name.push_back(c);
				}

				in->read(tmp, 4);
				len = read_uint32_be(tmp);

				qint64 pos = in->pos();

				uint16_t resid = read_uint16_be(irh.id);
				if (resid == 0x0409 || resid == 0x040c) {
					struct ThumbnailResourceHeader {
						uint32_t format;
						uint32_t width;
						uint32_t height;
						uint32_t widthbytes;
						uint32_t totalsize;
						uint32_t size_after_compression;
						uint16_t bits_per_pixel;
						uint16_t num_of_planes;
					};
					ThumbnailResourceHeader trh;
					in->read((char *)&trh, sizeof(ThumbnailResourceHeader));
					if (read_uint32_be(&trh.format) == 1) {
						uint32_t size_after_compression = read_uint32_be(&trh.size_after_compression);
						if (size_after_compression < 1000000) {
							jpeg->resize(size_after_compression);
							char *ptr = &jpeg->at(0);
							if (in->read(ptr, size_after_compression) == size_after_compression) {
								// ok
							} else {
								jpeg->clear();
							}
						}
					}
					break;
				}

				if (len & 1) len++;
				in->seek(pos + len);
			} else {
				break;
			}
		}
	}
}

