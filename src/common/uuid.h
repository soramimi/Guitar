#ifndef UUID_H
#define UUID_H

#include <cstdint>
#include <tuple>

std::pair<uint64_t, uint64_t> uuidv7();

void uuid_to_string(uint64_t hi, uint64_t lo, char *least37bytes);

#endif // UUID_H
