
#ifndef BASE64_H
#define BASE64_H

#include <vector>
#include <string>
#include <cctype>
#include <cstring>

class Base64 {
private:
	static unsigned char const PAD = '=';

	static unsigned char enc(int c)
	{
		static const unsigned char table[] = {
			0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50,
			0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
			0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76,
			0x77, 0x78, 0x79, 0x7a, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x2b, 0x2f,
		};
		return table[c & 63];
	}

	static unsigned char dec(int c)
	{
		static const unsigned char table[] = {
			0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
			0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
			0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x3e, 0xff, 0xff, 0xff, 0x3f,
			0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
			0xff, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
			0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0xff, 0xff, 0xff, 0xff, 0xff,
			0xff, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
			0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0xff, 0xff, 0xff, 0xff, 0xff,
		};
		return table[c & 127];
	}
public:
	static void encode(char const *src, size_t length, std::vector<char> *out)
	{
		size_t srcpos, dstlen, dstpos;

		dstlen = (length + 2) / 3 * 4;
		out->resize(dstlen);
		if (dstlen == 0) {
			return;
		}
		char *dst = &out->at(0);
		dstpos = 0;
		for (srcpos = 0; srcpos < length; srcpos += 3) {
			int v = (unsigned char)src[srcpos] << 16;
			if (srcpos + 1 < length) {
				v |= (unsigned char)src[srcpos + 1] << 8;
				if (srcpos + 2 < length) {
					v |= (unsigned char)src[srcpos + 2];
					dst[dstpos + 3] = enc(v);
				} else {
					dst[dstpos + 3] = PAD;
				}
				dst[dstpos + 2] = enc(v >> 6);
			} else {
				dst[dstpos + 2] = PAD;
				dst[dstpos + 3] = PAD;
			}
			dst[dstpos + 1] = enc(v >> 12);
			dst[dstpos] = enc(v >> 18);
			dstpos += 4;
		}
	}

	static bool decode(char const *src, size_t length, std::vector<char> *out)
	{
		out->clear();
		if (!src && length != 0) return false;

		unsigned char quartet[4];
		size_t count = 0;
		bool finished = false;
		for (size_t i = 0; i < length; ++i) {
			unsigned char ch = (unsigned char)src[i];
			if (std::isspace(ch)) continue;
			if (finished) return false;
			if (ch == PAD) {
				quartet[count++] = 0x40;
			} else {
				if (ch >= 0x80) return false;
				unsigned char value = dec(ch);
				if (value >= 0x40) return false;
				quartet[count++] = value;
			}
			if (count != 4) continue;

			if (quartet[0] >= 0x40 || quartet[1] >= 0x40) return false;
			if (quartet[2] == 0x40 && quartet[3] != 0x40) return false;
			out->push_back(char((quartet[0] << 2) | (quartet[1] >> 4)));
			if (quartet[2] != 0x40) {
				out->push_back(char((quartet[1] << 4) | (quartet[2] >> 2)));
				if (quartet[3] != 0x40) {
					out->push_back(char((quartet[2] << 6) | quartet[3]));
				}
			}
			finished = quartet[2] == 0x40 || quartet[3] == 0x40;
			count = 0;
		}
		if (count != 0) {
			out->clear();
			return false;
		}
		return true;
	}

	static std::string _to_s_(std::vector<char> const *vec)
	{
		if (!vec || vec->empty()) return std::string();
		return std::string((char const *)&(*vec)[0], vec->size());
	}
};

static inline void base64_encode(char const *src, size_t length, std::vector<char> *out)
{
	Base64::encode(src, length, out);
}

static inline bool base64_decode(char const *src, size_t length, std::vector<char> *out)
{
	return Base64::decode(src, length, out);
}

static inline std::string base64_encode(std::string_view const &src)
{
	std::vector<char> vec;
	base64_encode((char const *)src.data(), src.size(), &vec);
	return Base64::_to_s_(&vec);
}

static inline std::string base64_decode(std::string_view const &src)
{
	std::vector<char> vec;
	base64_decode((char const *)src.data(), src.size(), &vec);
	return Base64::_to_s_(&vec);
}

static inline std::string base64_encode_s(std::string_view src)
{
	std::vector<char> vec;
	base64_encode((char const *)src.data(), src.size(), &vec);
	return Base64::_to_s_(&vec);
}

#endif

