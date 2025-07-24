#ifndef CRC32_H
#define CRC32_H

#include <cstdint>
#include <cstddef>

namespace crc {
uint32_t crc32(uint32_t in, void const *data, size_t size);
}

#endif // CRC32_H
