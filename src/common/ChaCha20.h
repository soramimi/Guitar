#ifndef CHACHA20_H
#define CHACHA20_H

#include <cstdint>

class ChaCha20 {
protected:
	static constexpr int BUFFER_SIZE = 16;
	uint8_t key_[32] = { 0 };
	uint8_t nonce_[12] = { 0 };
	uint32_t state_[BUFFER_SIZE] = { 0 };
	uint32_t buffer_[BUFFER_SIZE] = { 0 };
	int index_ = 0;
	void seed_zero();
	void seed_random();
	void init_state();
public:
	ChaCha20();
	uint32_t next_u32();
};

#endif // CHACHA20_H
