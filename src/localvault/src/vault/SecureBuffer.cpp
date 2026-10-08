#include "SecureBuffer.h"

#include <new>
#include <sodium.h>

namespace localvault {

namespace secure_memory {

	void *allocate(size_t size)
	{
		static const bool ready = sodium_init() >= 0;
		if (!ready) throw std::bad_alloc();
		void *p = sodium_malloc(size == 0 ? 1 : size);
		if (!p) throw std::bad_alloc();
		return p;
	}

	void deallocate(void *p) noexcept
	{
		if (p) sodium_free(p);
	}

	void zero(void *p, size_t size) noexcept
	{
		if (p && size > 0) sodium_memzero(p, size);
	}

} // namespace secure_memory

namespace {

	/**
	 * @brief コンパイラの最適化を避けて確実にメモリをゼロクリアする
	 */
	void secure_memzero(void *p, size_t n)
	{
		if (!p || n == 0) {
			return;
		}

		sodium_memzero(p, n);
	}

	bool secure_lock(void *p, size_t n)
	{
		if (!p || n == 0) {
			return false;
		}
		return sodium_mlock(p, n) == 0;
	}

	bool secure_unlock(void *p, size_t n)
	{
		if (!p || n == 0) {
			return false;
		}
		return sodium_munlock(p, n) == 0;
	}

} // namespace

SecureBuffer::SecureBuffer(size_t size)
	: data_(size)
{
}

SecureBuffer::SecureBuffer(void const *data, size_t size)
	: data_(static_cast<uint8_t const *>(data), static_cast<uint8_t const *>(data) + size)
{
}

SecureBuffer::SecureBuffer(uint8_t const *begin, uint8_t const *end)
	: data_(begin, end)
{
}

SecureBuffer::SecureBuffer(SecureBuffer &&other) noexcept
	: data_(std::move(other.data_))
	, locked_(other.locked_)
{
	other.locked_ = false;
}

SecureBuffer &SecureBuffer::operator=(SecureBuffer &&other) noexcept
{
	if (this != &other) {
		if (locked_) {
			secure_unlock(data_.data(), data_.size());
		}
		secure_memzero(data_.data(), data_.size());
		data_ = std::move(other.data_);
		locked_ = other.locked_;
		other.locked_ = false;
	}
	return *this;
}

SecureBuffer::~SecureBuffer()
{
	if (locked_) {
		secure_unlock(data_.data(), data_.size());
	}
	secure_memzero(data_.data(), data_.size());
}

bool SecureBuffer::empty() const
{
	return data_.empty();
}

size_t SecureBuffer::size() const
{
	return data_.size();
}

bool SecureBuffer::equals(SecureBuffer const &other) const
{
	return data_.size() == other.data_.size()
		&& (data_.empty() || sodium_memcmp(data_.data(), other.data_.data(), data_.size()) == 0);
}

uint8_t *SecureBuffer::data()
{
	return data_.data();
}

uint8_t const *SecureBuffer::data() const
{
	return data_.data();
}

uint8_t *SecureBuffer::begin()
{
	return data_.data();
}

uint8_t const *SecureBuffer::begin() const
{
	return data_.data();
}

uint8_t *SecureBuffer::end()
{
	return data_.data() + data_.size();
}

uint8_t const *SecureBuffer::end() const
{
	return data_.data() + data_.size();
}

uint8_t &SecureBuffer::operator[](size_t index)
{
	return data_[index];
}

const uint8_t &SecureBuffer::operator[](size_t index) const
{
	return data_[index];
}

bool SecureBuffer::resize(size_t size)
{
	if (locked_ && size != data_.size()) {
		return false;
	}
	if (size < data_.size()) {
		// 縮小時は切り捨てられる領域をゼロクリア
		secure_memzero(data_.data() + size, data_.size() - size);
	}
	data_.resize(size);
	return true;
}

void SecureBuffer::clear()
{
	secure_memzero(data_.data(), data_.size());
	unlock();
	data_.clear();
}

bool SecureBuffer::assign(void const *data, size_t size)
{
	if (locked_) {
		return false;
	}
	secure_memzero(data_.data(), data_.size());
	data_.assign(static_cast<uint8_t const *>(data), static_cast<uint8_t const *>(data) + size);
	return true;
}

bool SecureBuffer::append(void const *data, size_t size)
{
	if (locked_) {
		return false;
	}
	uint8_t const *begin = static_cast<uint8_t const *>(data);
	data_.insert(data_.end(), begin, begin + size);
	return true;
}

bool SecureBuffer::lock()
{
	if (locked_ || data_.empty()) {
		return locked_;
	}
	locked_ = secure_lock(data_.data(), data_.size());
	return locked_;
}

void SecureBuffer::unlock()
{
	if (!locked_ || data_.empty()) {
		return;
	}
	secure_unlock(data_.data(), data_.size());
	locked_ = false;
}

} // namespace localvault
