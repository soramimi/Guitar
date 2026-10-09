#ifndef VAULT_H
#define VAULT_H

#include "SecretStorage.h"
#include "SecureBuffer.h"
#include <cstdint>
#include <string>

namespace localvault {

/**
 * @brief Vault 操作の結果
 *
 * 呼び出し側（GUI 等）が原因ごとに適切な案内を出せるよう、失敗理由を区別する。
 * 特に BackendUnavailable / StorageError を WrongPin と取り違えると、
 * ユーザーを不要なリセットへ誘導してしまうため注意すること。
 */
enum class VaultError {
	None, ///< 成功
	InvalidArgument, ///< 空の PIN 等
	CryptoInitFailed, ///< libsodium の初期化失敗
	BackendUnavailable, ///< ストレージに到達できない・ロックされている
	StorageError, ///< ストレージの読み書き失敗、書き込み内容の検証失敗
	NotSetup, ///< EMK が存在しない
	AlreadySetup, ///< EMK が既に存在する（setup() は上書きしない）
	Locked, ///< unlock されていない
	WrongPin, ///< EMK の認証失敗（PIN の誤り。EMK 改ざんの場合も同じ結果になる）
	CorruptedData, ///< 形式不正・未対応バージョン・パラメータ範囲外
	AuthenticationFailed, ///< 暗号文の認証失敗（改ざん、または別の MK で暗号化されたデータ）
	KeyDerivationFailed, ///< Argon2id の失敗（メモリ不足等）
	MemoryLockFailed ///< 鍵用メモリのロック失敗（RLIMIT_MEMLOCK 等）
};

/** VaultError の英語の説明（ログ用。UI 文言は呼び出し側で用意すること） */
const char *toString(VaultError error);

/**
 * @brief Vault の現在の状態
 */
enum class VaultState {
	NotSetup, ///< EMK が存在しない
	Locked, ///< EMK は存在するが MK はメモリ上にない
	Unlocked, ///< MK がメモリ上にある
	BackendUnavailable, ///< ストレージに到達できず EMK の有無が不明
	StorageError ///< ストレージエラーで EMK の有無が不明
};

/**
 * @brief ローカル Vault（機密情報管理コア）
 *
 * 2段階鍵体系（MK / KEK / EMK）を採用。
 * - MK: ランダム 256bit。データの暗号化に使用
 * - KEK: PIN から Argon2id で導出
 * - EMK: KEK で暗号化された MK。ISecretStorageBackend に保存
 *
 * EMK の書き換え（setup / changePin / reset）は、まずステージング用キー
 * （emkKey + ".pending"）へ書き込んで読み戻し検証し、その後で本キーを置き換える。
 * 本キーの置き換えに失敗しても、旧 EMK は本キーに、新 EMK はステージングに残るため、
 * 旧 PIN・新 PIN のどちらでも解除できる（新 PIN で解除した場合は本キーへ昇格する）。
 *
 * スレッドセーフではない。Qt 非依存。
 */
class Vault {
private:
	ISecretStorageBackend *backend_;
	std::string emkKey_;
	std::string pendingKey_;
	SecureBuffer mk_;
	bool unlocked_ = false;

	VaultError deriveKek(SecureBuffer const &pin, Blob const &salt, uint32_t opsLimit, uint32_t memLimit, SecureBuffer *kek) const;
	VaultError createEmk(SecureBuffer const &mk, SecureBuffer const &pin, Blob *emk) const;
	VaultError parseEmk(Blob const &emk, SecureBuffer const &pin, SecureBuffer *mk) const;
	VaultError openEmk(SecureBuffer const &pin, SecureBuffer *mk);
	VaultError writeEmk(Blob const &emk, bool createOnly);
	VaultError encryptBytes(unsigned char const *plain, size_t size, Blob *cipher);
	VaultError storeVerified(std::string const &key, Blob const &data);

	static VaultError newMasterKey(SecureBuffer *mk);

public:
	/**
	 * @param backend EMK を保存するバックエンド。生存期間中は有効なポインタであること
	 * @param emkKey バックエンド内での EMK の識別キー
	 */
	Vault(ISecretStorageBackend *backend, std::string const &emkKey = "vault_emk");
	~Vault();

	Vault(const Vault &) = delete;
	Vault &operator=(const Vault &) = delete;

	bool isAvailable() const;
	bool isUnlocked() const;

	/**
	 * @brief バックエンドを参照して現在の状態を返す
	 *
	 * BackendUnavailable / StorageError の場合は EMK の有無が不明であり、
	 * 新規セットアップへ進んではならない。
	 */
	VaultState state() const;

	/** バックエンド内でEMKを識別するキー */
	std::string const &emkKey() const { return emkKey_; }

	/**
	 * @brief 初回セットアップ。PIN から KEK を導出し、MK を生成・暗号化して保存する
	 *
	 * EMK が既に存在する場合は AlreadySetup を返し、上書きしない。
	 */
	[[nodiscard]] VaultError setup(SecureBuffer const &pin);

	/**
	 * @brief PIN で Vault を解除し、MK をメモリ上に復号する
	 */
	[[nodiscard]] VaultError unlock(SecureBuffer const &pin);

	/**
	 * @brief メモリ上の MK を消去する
	 */
	void lock();

	/**
	 * @brief 平文を暗号化する。unlock 済みである必要がある
	 *
	 * 機密データ（ユーザーが入力した API キー等）はこちらを使い、通常メモリを経由させないこと。
	 */
	[[nodiscard]] VaultError encrypt(SecureBuffer const &plain, Blob *cipher);

	/**
	 * @brief 平文を暗号化する（機密性の低いデータ、または既に通常メモリ上にあるデータ用）
	 */
	[[nodiscard]] VaultError encrypt(Blob const &plain, Blob *cipher);

	/**
	 * @brief 暗号文を復号する。unlock 済みである必要がある
	 *
	 * 平文は SecureBuffer に返され、使用後は明示的に clear() すること。
	 */
	[[nodiscard]] VaultError decryptToSecureBuffer(Blob const &cipher, SecureBuffer *plain);

	/**
	 * @brief PIN を変更する。unlock 済みである必要がある
	 *
	 * StorageError が返った場合でも、本キーの置き換えだけが失敗している可能性がある。
	 * その場合は旧 PIN・新 PIN のどちらでも解除できる。
	 */
	[[nodiscard]] VaultError changePin(SecureBuffer const &oldPin, SecureBuffer const &newPin);

	/**
	 * @brief 新しい MK と PIN で EMK を置き換える。
	 *
	 * 既存のMKで暗号化されたデータは復号不能になる不可逆操作。
	 * 既存 EMK は新 EMK の書き込みに成功するまで削除しない。
	 */
	[[nodiscard]] VaultError reset(SecureBuffer const &newPin);

	/**
	 * @brief EMK（ステージングを含む）を削除し、ロックする。不可逆操作
	 */
	[[nodiscard]] VaultError destroy();
};

} // namespace localvault

#endif // VAULT_H
