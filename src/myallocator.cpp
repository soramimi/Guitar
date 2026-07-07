#include "myallocator.h"

#ifdef USE_JEMALLOC
#include <new>
#include <jemalloc/jemalloc.h>

void *operator new (std::size_t size)
{
	void *p = je_malloc(size);
	if (!p) throw std::bad_alloc();
	return p;
}

void *operator new[] (std::size_t size)
{
	void *p = je_malloc(size);
	if (!p) throw std::bad_alloc();
	return p;
}

void operator delete (void *p) noexcept
{
	je_free(p);
}

void operator delete[] (void *p) noexcept
{
	je_free(p);
}

void operator delete (void *p, std::size_t size) noexcept
{
	je_free(p);
}

void operator delete[] (void *p, std::size_t size) noexcept
{
	je_free(p);
}


#endif

