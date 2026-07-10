#ifndef MYALLOCATOR_H
#define MYALLOCATOR_H

#if 1//def USE_JEMALLOC

#include <stdlib.h>
#include <new>

void *operator new (std::size_t size);
void *operator new[] (std::size_t size);
void operator delete (void *p) noexcept;
void operator delete[] (void *p) noexcept;
void operator delete (void *p, std::size_t size) noexcept;
void operator delete[] (void *p, std::size_t size) noexcept;

void *operator new(std::size_t size, std::align_val_t alignment);
void *operator new[](std::size_t size, std::align_val_t alignment);
void operator delete(void *ptr, std::align_val_t alignment) noexcept;
void operator delete[](void *ptr, std::align_val_t alignment) noexcept;
void operator delete(void *ptr, std::size_t size, std::align_val_t alignment) noexcept;
void operator delete[](void *ptr, std::size_t size, std::align_val_t alignment) noexcept;

void *operator new(std::size_t size, const std::nothrow_t &) noexcept;
void operator delete(void *ptr, const std::nothrow_t &) noexcept;

#endif

#endif // MYALLOCATOR_H
