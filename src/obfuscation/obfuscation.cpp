#include "obfuscation.h"
#include "../common/crc32.h"
#include <cstring>
#include "../common/ChaCha20.h"

// This is obfuscation, not encryption.

namespace {

class KeyGenerator : ChaCha20 {
private:
#if 0
	uint32_t xorshift32_state_ = 12345678;
	
	uint32_t xorshift32()
	{
		uint32_t x = xorshift32_state_;
		x ^= x << 13;
		x ^= x >> 17;
		x ^= x << 5;
		return xorshift32_state_ = x;
	}

	uint32_t next_u32()
	{
		return xorshift32();
	}
#endif
private:
	uint32_t bytes_ = 0;
	short remain_ = 0;
	uint32_t salt_ = 0;
	
	void _reset_obfuscator()
	{
		bytes_ = 0;
		remain_ = 0;
		seed_zero();
		memcpy(key_, "It is obfuscation not encryption", 32);
		memcpy(nonce_, &salt_, 4);
		init_state();
	}
public:
	void reset_random()
	{
		seed_random();
		init_state();
	}
	uint32_t reset_obfuscator_for_encoder()
	{
		salt_ = next_u32();
		_reset_obfuscator();
		return salt_;
	}
	void reset_obfuscator_for_decoder(uint32_t salt)
	{
		salt_ = salt;
		_reset_obfuscator();
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
	uint32_t size = 0;
	uint32_t crc32 = 0;
	uint32_t salt = 0;
};

void _encdec(std::string_view source, char *out, size_t len, KeyGenerator *keygen)
{
	assert(source.size() == len);
	for (char c : source) {
		uint8_t key = keygen->next_u8();
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
	KeyGenerator keygen;
	header->salt = keygen.reset_obfuscator_for_encoder();
	_encdec(std::string_view(source.constData(), source.size()), out.data() + sizeof(Header), source.size(), &keygen);
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
	KeyGenerator keygen;
	keygen.reset_obfuscator_for_decoder(header->salt);
	_encdec(std::string_view(encoded.constData() + sizeof(Header), out.size()), out.data(), out.size(), &keygen);
	uint32_t crc = crc32(0, encoded.constData() + sizeof(Header), header->size);
	if (crc != header->crc32) {
		return {};
	}
	return out;
}


