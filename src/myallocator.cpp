#include "myallocator.h"

#ifdef USE_JEMALLOC
#include <jemalloc/jemalloc.h>
#endif

#include <new>

#include <stdio.h>
#include <string.h>

static unsigned int n = 0;
#define CHECK_POINT 16

static inline void *x_malloc(std::size_t size)
{
	n++;
	if (n == CHECK_POINT) {
		fprintf(stderr, "");
	}
	char *q = (char *)je_malloc(size + 32);
	char *p = q + 16;
	fprintf(stderr, "--- [+] %p %d\n", p, n);
	memset(q, 0x55, 16);
	memset(p + size, 0xaa, 16);
	*(unsigned int *)q = n;
	return p;
}

static inline void x_free(void *p)
{
	if (!p) return;
	char *q = (char *)p - 16;
	unsigned int m = *(unsigned int *)(q);
	if (m == CHECK_POINT) {
		fprintf(stderr, "");
	}
	fprintf(stderr, "--- [-] %p %d\n", p, m);
	je_free(q);
}

void *operator new (std::size_t size)
{
	void *p = x_malloc(size);
	if (!p) throw std::bad_alloc();
	return p;
}

void *operator new[] (std::size_t size)
{
	void *p = x_malloc(size);
	if (!p) throw std::bad_alloc();
	return p;
}

void operator delete (void *p) noexcept
{
	x_free(p);
}

void operator delete[] (void *p) noexcept
{
	x_free(p);
}

void operator delete (void *p, std::size_t size) noexcept
{
	x_free(p);
}

void operator delete[] (void *p, std::size_t size) noexcept
{
	x_free(p);
}

void *operator new(std::size_t size, std::align_val_t alignment)
{
	void *p = x_malloc(size);
	if (!p) throw std::bad_alloc();
	return p;
}

void *operator new[](std::size_t size, std::align_val_t alignment)
{
	void *p = x_malloc(size);
	if (!p) throw std::bad_alloc();
	return p;
}

void operator delete(void *ptr, std::align_val_t alignment) noexcept
{
	x_free(ptr);
}

void operator delete[](void *ptr, std::align_val_t alignment) noexcept
{
	x_free(ptr);
}

void operator delete(void *ptr, std::size_t size, std::align_val_t alignment) noexcept
{
	x_free(ptr);
}

void operator delete[](void *ptr, std::size_t size, std::align_val_t alignment) noexcept
{
	x_free(ptr);
}

void *operator new(std::size_t size, const std::nothrow_t &) noexcept
{
	void *p = x_malloc(size);
	return p;
}

void operator delete(void *ptr, const std::nothrow_t &) noexcept
{
	x_free(ptr);
}
