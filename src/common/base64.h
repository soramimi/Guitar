
#ifndef BASE64_H
#define BASE64_H

#include <vector>
#include <string>
#include <cstdint>

class Base64 {
private:
	static uint8_t const PAD = '=';

	static uint8_t enc(int c)
	{
		static const uint8_t table[] = {
			0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50,
			0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
			0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76,
			0x77, 0x78, 0x79, 0x7a, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x2b, 0x2f,
		};
		return table[c & 63];
	}

	static uint8_t dec(int c)
	{
		static const uint8_t table[] = {
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
	static std::vector<char> encode(uint8_t const *src, size_t len)
	{
		if (!src || len == 0) return {};

		size_t srcpos, dstlen, dstpos;
		dstlen = (len + 2) / 3 * 4;
	
		std::vector<char> ret;
		ret.resize(dstlen);
		
		uint8_t *dst = (uint8_t *)ret.data();
		dstpos = 0;
		for (srcpos = 0; srcpos < len; srcpos += 3) {
			int v = src[srcpos] << 16;
			if (srcpos + 1 < len) {
				v |= src[srcpos + 1] << 8;
				if (srcpos + 2 < len) {
					v |= src[srcpos + 2];
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
		return ret;
	}

	static std::vector<char> decode(uint8_t const *src, size_t len)
	{
		std::vector<char> ret;
		ret.reserve(len * 3 / 4);
		
		uint8_t const *begin = (uint8_t const *)src;
		uint8_t const *end = begin + len;
		uint8_t const *ptr = begin;
		int count = 0;
		int bits = 0;
		while (1) {
			if (isspace(*ptr)) {
				ptr++;
			} else {
				uint8_t c = 0xff;
				if (ptr < end && *ptr < 0x80) {
					c = dec(*ptr);
				}
				if (c < 0x40) {
					bits = (bits << 6) | c;
					count++;
				} else {
					if (count < 4) {
						bits <<= (4 - count) * 6;
					}
					c = 0xff;
				}
				if (count == 4 || c == 0xff) {
					if (count >= 2) {
						ret.push_back(bits >> 16);
						if (count >= 3) {
							ret.push_back(bits >> 8);
							if (count == 4) {
								ret.push_back(bits);
							}
						}
					}
					count = 0;
					bits = 0;
					if (c == 0xff) {
						break;
					}
				}
				ptr++;
			}
		}
		return ret;
	}

	static std::string _to_s_(std::vector<char> const &vec)
	{
		if (vec.empty()) return {};
		return std::string(vec.data(), vec.size());
	}
};

static inline std::vector<char> base64_encode_v(std::vector<char> const &v)
{
	return Base64::encode((uint8_t const *)v.data(), v.size());
}

static inline std::vector<char> base64_decode_v(std::vector<char> const &v)
{
	return Base64::decode((uint8_t const *)v.data(), v.size());
}

static inline std::vector<char> base64_encode_v(std::string_view s)
{
	return Base64::encode((uint8_t const *)s.data(), s.size());
}

static inline std::vector<char> base64_decode_v(std::string_view s)
{
	return Base64::decode((uint8_t const *)s.data(), s.size());
}

static inline std::string base64_encode_s(std::string_view s)
{
	std::vector<char> vec = base64_encode_v(s);
	return Base64::_to_s_(vec);
}

static inline std::string base64_decode_s(std::string_view s)
{
	std::vector<char> vec = base64_decode_v(s);
	return Base64::_to_s_(vec);
}

#endif

