#ifndef UUID_H
#define UUID_H

#include <cstdint>
#include <tuple>
#include <string>

std::pair<uint64_t, uint64_t> uuidv7();

void uuid_to_string(uint64_t hi, uint64_t lo, char *least37bytes);

inline std::string generate_uuidv7()
{
	auto [hi, lo] = uuidv7();
	char buf[37];
	uuid_to_string(hi, lo, buf);
	return buf;
}

#endif // UUID_H
