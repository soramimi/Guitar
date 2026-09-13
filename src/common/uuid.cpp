#include "uuid.h"
#include "ChaCha20.h"
#include <chrono>

namespace {

ChaCha20 rng;

inline uint64_t now_us()
{
	auto now = std::chrono::system_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch());
	return duration.count();
}

inline uint32_t rand_u32()
{
	return rng.next_u32();
}

inline void write04x(char *out, uint16_t v)
{
	static const char hex[] = "0123456789abcdef";
	out[0] = hex[(v >> 12) & 0xf];
	out[1] = hex[(v >> 8) & 0xf];
	out[2] = hex[(v >> 4) & 0xf];
	out[3] = hex[v & 0xf];
}

} // namespace

std::pair<uint64_t, uint64_t> uuidv7()
{
	uint64_t t = now_us();
	uint64_t ms = t / 1000;
	uint64_t us = t % 1000;
	uint32_t rand_hi = rand_u32();
	uint32_t rand_lo = rand_u32();
	uint64_t uuid_hi = (ms << 16) | 0x7000 | (us << 2) | (rand_hi >> 30);
	uint64_t uuid_lo = 0x8000000000000000ULL | ((rand_hi & 0x3fffffffULL) << 32) | rand_lo;
	return {uuid_hi, uuid_lo};
}

void uuid_to_string(uint64_t hi, uint64_t lo, char *least37bytes)
{
	char *o = least37bytes;
	write04x(o + 0, uint16_t(hi >> 48));
	write04x(o + 4, uint16_t(hi >> 32));
	o[8] = '-';
	write04x(o + 9, uint16_t(hi >> 16));
	o[13] = '-';
	write04x(o + 14, uint16_t(hi >> 0));
	o[18] = '-';
	write04x(o + 19, uint16_t(lo >> 48));
	o[23] = '-';
	write04x(o + 24, uint16_t(lo >> 32));
	write04x(o + 28, uint16_t(lo >> 16));
	write04x(o + 32, uint16_t(lo >> 0));
	o[36] = 0;
}
