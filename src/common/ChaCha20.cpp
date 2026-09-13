#include "ChaCha20.h"
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef __linux__
#include <sys/random.h>

static void fill_random(void *p, size_t len)
{
	auto *out = static_cast<uint8_t *>(p);

	while (len > 0) {
		ssize_t n = getrandom(out, len, 0);
		if (n <= 0) {
			if (n < 0 && errno == EINTR) continue;
			// Running with a zero/partial key would make the CSPRNG fully
			// predictable, so fail hard instead of continuing silently.
			fprintf(stderr, "getrandom failed: %s\n", n < 0 ? strerror(errno) : "unexpected end");
			abort();
		}
		out += n;
		len -= n;
	}
}
#endif

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
static void fill_random(void *p, size_t len)
{
	NTSTATUS status = BCryptGenRandom(nullptr, (PUCHAR)p, len, BCRYPT_USE_SYSTEM_PREFERRED_RNG);

	if (status < 0) {
		fprintf(stderr, "BCryptGenRandom failed: 0x%08x\n", status);
		abort();
		return;
	}
}
#endif

static uint32_t rotl(uint32_t v, int n)
{
	return (v << n) | (v >> (32 - n));
}

static void quarter_round(uint32_t *x, int a, int b, int c, int d)
{
	x[a] += x[b];
	x[d] ^= x[a];
	x[d] = rotl(x[d], 16);
	x[c] += x[d];
	x[b] ^= x[c];
	x[b] = rotl(x[b], 12);
	x[a] += x[b];
	x[d] ^= x[a];
	x[d] = rotl(x[d], 8);
	x[c] += x[d];
	x[b] ^= x[c];
	x[b] = rotl(x[b], 7);
}

// Produce one 64-byte block as 16 uint32 words. Working with words directly
// (instead of serializing to bytes and reading them back) keeps next_u32()
// independent of the host byte order.
static void chacha20_block(const uint32_t in[16], uint32_t out[16])
{
	uint32_t x[16];
	memcpy(x, in, sizeof(x));

	for (int i = 0; i < 10; i++) {
		quarter_round(x, 0, 4, 8, 12);
		quarter_round(x, 1, 5, 9, 13);
		quarter_round(x, 2, 6, 10, 14);
		quarter_round(x, 3, 7, 11, 15);
		quarter_round(x, 0, 5, 10, 15);
		quarter_round(x, 1, 6, 11, 12);
		quarter_round(x, 2, 7, 8, 13);
		quarter_round(x, 3, 4, 9, 14);
	}

	for (int i = 0; i < 16; i++) {
		out[i] = x[i] + in[i];
	}
}

static void chacha20_init_state(uint32_t state[16], const uint8_t key[32], const uint8_t iv[12], uint32_t counter)
{
	state[0] = 0x61707865;
	state[1] = 0x3320646e;
	state[2] = 0x79622d32;
	state[3] = 0x6b206574;

	for (int i = 0; i < 8; i++) {
		state[4 + i] = key[i * 4 + 0] | (key[i * 4 + 1] << 8) | (key[i * 4 + 2] << 16) | (key[i * 4 + 3] << 24);
	}

	state[12] = counter;

	for (int i = 0; i < 3; i++) {
		state[13 + i] = iv[i * 4 + 0] | (iv[i * 4 + 1] << 8) | (iv[i * 4 + 2] << 16) | (iv[i * 4 + 3] << 24);
	}
}

void ChaCha20::seed_zero()
{
	memset(key_, 0, sizeof(key_));
	memset(nonce_, 0, sizeof(nonce_));
}

void ChaCha20::seed_random()
{
	fill_random(key_, sizeof(key_));
	fill_random(nonce_, sizeof(nonce_));
}

void ChaCha20::init_state()
{
	chacha20_init_state(state_, key_, nonce_, 0);
}

ChaCha20::ChaCha20()
{
	seed_random();
	init_state();
}

uint32_t ChaCha20::next_u32()
{
	if (index_ == 0) {
		chacha20_block(state_, buffer_);
		state_[12]++;
	}
	uint32_t value = buffer_[index_];
	index_ = (index_ + 1) % BUFFER_SIZE;
	return value;
}
