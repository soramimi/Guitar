#ifndef SECUREBUFFER_H
#define SECUREBUFFER_H

#include <cstddef>
#include <cstdint>
#include <vector>

class SecurePinEdit;
class LocalVaultTest;

namespace localvault {

#ifdef VAULT_ALLOW_EMPTY_PIN
constexpr bool allow_empty_pin = true;
#else
constexpr bool allow_empty_pin = false;
#endif

namespace secure_memory {

	/** sodium_malloc で確保する。失敗時は std::bad_alloc を送出 */
	void *allocate(size_t size);
	/** sodium_free で解放する（ゼロクリア・ロック解除を伴う） */
	void deallocate(void *p) noexcept;
	/** 最適化で省略されないゼロクリア（スタック上の一時領域等に使う） */
	void zero(void *p, size_t size) noexcept;

	/**
	 * @brief libsodium のガード付きメモリを使う STL アロケータ
	 *
	 * std::vector が再確保する際、古い領域は sodium_free によりゼロクリアされてから
	 * 解放されるため、拡張時に機密データの断片がヒープに残らない。
	 * 確保領域はガードページで保護され、ベストエフォートでメモリロックされる。
	 */
	template <typename T> struct Allocator {
		using value_type = T;

		Allocator() noexcept = default;
		template <typename U> Allocator(Allocator<U> const &) noexcept { }

		T *allocate(size_t n) { return static_cast<T *>(secure_memory::allocate(n * sizeof(T))); }
		void deallocate(T *p, size_t) noexcept { secure_memory::deallocate(p); }

		template <typename U> bool operator==(Allocator<U> const &) const noexcept { return true; }
		template <typename U> bool operator!=(Allocator<U> const &) const noexcept { return false; }
	};

} // namespace secure_memory

/**
 * @brief 機密情報を保持するためのセキュアメモリバッファ
 *
 * 通常の std::vector とは異なり、以下の特性を持つ:
 * - 解放時・再確保時に確実にゼロクリア（libsodium のガード付きメモリを使用）
 * - 可能であればメモリをロックしてスワップアウトを防止
 * - コピー禁止（ムーブのみ可）
 */
class SecureBuffer {
	friend class Vault;
	friend class ::SecurePinEdit;
	friend class ::LocalVaultTest;

private:
	std::vector<uint8_t, secure_memory::Allocator<uint8_t>> data_;
	bool locked_ = false;

	/**
	 * @brief ページングファイルへの書き出しを防ぐためメモリをロックする
	 * @return ロックに成功した場合 true
	 */
	bool lock();

	/**
	 * @brief lock() でロックしたメモリを解除する
	 *
	 * 注意: sodium_munlock の仕様により、ロック解除と同時に内容はゼロクリアされる。
	 * サイズは変わらないが、以後の内容はすべて 0 になる。
	 */
	void unlock();

public:
	SecureBuffer() = default;
	explicit SecureBuffer(size_t size);
	SecureBuffer(void const *data, size_t size);
	SecureBuffer(uint8_t const *begin, uint8_t const *end);
	SecureBuffer(SecureBuffer &&other) noexcept;
	SecureBuffer &operator=(SecureBuffer &&other) noexcept;
	~SecureBuffer();

	SecureBuffer(SecureBuffer const &) = delete;
	SecureBuffer &operator=(SecureBuffer const &) = delete;

	bool empty() const;
	size_t size() const;

	/** 内容が等しいかを定数時間で比較する（サイズが異なる場合は false） */
	bool equals(SecureBuffer const &other) const;

	uint8_t *data();
	uint8_t const *data() const;

	uint8_t *begin();
	uint8_t const *begin() const;
	uint8_t *end();
	uint8_t const *end() const;

	uint8_t &operator[](size_t index);
	const uint8_t &operator[](size_t index) const;

	/** Structural changes are refused while the buffer is memory-locked. */
	bool resize(size_t size);
	void clear();
	bool assign(void const *data, size_t size);
	bool append(void const *data, size_t size);
};

static inline bool validate_pin(SecureBuffer const &pin)
{
	if (!allow_empty_pin && pin.empty()) {
		return false;
	}
	return true;
}

} // namespace localvault

#endif // SECUREBUFFER_H
