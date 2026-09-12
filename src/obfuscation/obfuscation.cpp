#include "obfuscation.h"
#include "../common/ChaCha20.h"
#include "../common/crc32.h"
#include <cstring>

namespace {

class KeyGenerator : ChaCha20 {
private:
	uint32_t bytes_ = 0;
	int remain_ = 0;
public:
	KeyGenerator()
	{
		seed_zero();
		init_state();
		memcpy(key_, "It is obfuscation not encryption", 32);
		memcpy(nonce_, "Hello, world", 12);
	}
	uint8_t next_u8()
	{
		if (remain_ == 0) {
			bytes_ = next_u32();
			remain_ = 4;
		}
		uint8_t b = bytes_ & 0xff;
		bytes_ >>= 8;
		remain_--;
		return b;
	}

};

struct Header {
	uint8_t magic[4] = {};
	uint32_t reserved = 0;
	uint32_t size = 0;
	uint32_t crc32 = 0;
};

void _encdec(std::string_view source, char *out, size_t len)
{
	assert(source.size() == len);
	KeyGenerator keygen;
	for (char c : source) {
		uint8_t key = keygen.next_u8();
		*out++ = c ^ key;
	}
}

} // namespace


QByteArray obfuscation::encode(QByteArray const &source, std::string_view magic_4bytes)
{
	if (magic_4bytes.size() != 4) {
		return {};
	}
	QByteArray out;
	out.resize(sizeof(Header) + source.size());
	Header *header = (Header *)out.data();
	*header = {};
	memcpy(header->magic, magic_4bytes.data(), magic_4bytes.size());
	_encdec(std::string_view(source.constData(), source.size()), out.data() + sizeof(Header), source.size());
	header->reserved = 0;
	header->size = source.size();
	header->crc32 = crc32(0, out.constData() + sizeof(Header), source.size());
	return out;
}

QByteArray obfuscation::decode(QByteArray const &encoded, std::string_view magic_4bytes)
{
	if (magic_4bytes.size() != 4) {
		return {};
	}
	if (encoded.size() < sizeof(Header)) {
		return {};
	}
	Header const *header = (Header const *)encoded.constData();
	if (memcmp(header->magic, magic_4bytes.data(), sizeof(header->magic)) != 0) {
		return {};
	}
	if (encoded.size() != sizeof(Header) + header->size) {
		return {};
	}
	QByteArray out;
	out.resize(encoded.size() - sizeof(Header));
	_encdec(std::string_view(encoded.constData() + sizeof(Header), out.size()), out.data(), out.size());
	uint32_t crc = crc32(0, encoded.constData() + sizeof(Header), header->size);
	if (crc != header->crc32) {
		return {};
	}
	return out;
}


