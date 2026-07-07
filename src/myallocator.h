#ifndef MYALLOCATOR_H
#define MYALLOCATOR_H

#ifdef USE_JEMALLOC

#include <stdlib.h>
#include <new>

void *operator new (std::size_t size);
void *operator new[] (std::size_t size);
void operator delete (void *p) noexcept;
void operator delete[] (void *p) noexcept;
void operator delete (void *p, std::size_t size) noexcept;
void operator delete[] (void *p, std::size_t size) noexcept;

#endif

#endif // MYALLOCATOR_H
