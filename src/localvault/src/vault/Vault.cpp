#include "Vault.h"

#include <algorithm>
#include <cstring>
#include <sodium.h>

namespace localvault {

namespace {

	constexpr char MAGIC[] = "VLT2";
	constexpr int MAGIC_SIZE = 4;
	constexpr uint8_t VERSION = 2;
	constexpr uint8_t EMK_RECORD = 1;
	constexpr uint8_t DATA_RECORD = 2;
	constexpr uint8_t XCHACHA20_POLY1305 = 1;
	constexpr uint8_t ARGON2ID = 2;
	constexpr int SALT_SIZE = crypto_pwhash_SALTBYTES;
	constexpr int NONCE_SIZE = crypto_aead_xchacha20poly1305_ietf_NPUBBYTES;
	constexpr int MK_SIZE = crypto_aead_xchacha20poly1305_ietf_KEYBYTES;
	constexpr int TAG_SIZE = crypto_aead_xchacha20poly1305_ietf_ABYTES;
	constexpr int EMK_HEADER_SIZE = MAGIC_SIZE + 1 + 1 + 1 + 1 + 4 + 4 + 1 + SALT_SIZE;
	constexpr int EMK_SIZE = EMK_HEADER_SIZE + NONCE_SIZE + MK_SIZE + TAG_SIZE;
	constexpr int DATA_HEADER_SIZE = MAGIC_SIZE + 1 + 1 + 1;
	// libsodium crypto_pwhash_OPSLIMIT_MODERATE / MEMLIMIT_MODERATE に相当
	constexpr uint32_t ARGON2_OPSLIMIT = 3;
	constexpr uint32_t ARGON2_MEMLIMIT = 256 * 1024 * 1024;
	constexpr uint32_t MIN_ARGON2_OPSLIMIT = 2;
	constexpr uint32_t MAX_ARGON2_OPSLIMIT = 10;
	constexpr uint32_t MIN_ARGON2_MEMLIMIT = 64 * 1024 * 1024;
	constexpr uint32_t MAX_ARGON2_MEMLIMIT = 1024 * 1024 * 1024;
	constexpr char PENDING_SUFFIX[] = ".pending";

	bool sodiumReady()
	{
		static const bool ready = sodium_init() >= 0;
		return ready;
	}

	void appendU32(Blob *out, uint32_t value)
	{
		out->push_back(static_cast<char>((value >> 24) & 0xff));
		out->push_back(static_cast<char>((value >> 16) & 0xff));
		out->push_back(static_cast<char>((value >> 8) & 0xff));
		out->push_back(static_cast<char>(value & 0xff));
	}

	uint32_t readU32(Blob const &in, size_t offset)
	{
		const auto *p = reinterpret_cast<unsigned char const *>(in.data() + offset);
		return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) | (static_cast<uint32_t>(p[2]) << 8) | p[3];
	}

	Blob generateRandom(size_t length)
	{
		Blob out(length);
		randombytes_buf(out.data(), out.size());
		return out;
	}

	VaultError toVaultError(StorageStatus status)
	{
		switch (status) {
		case StorageStatus::Ok:
			return VaultError::None;
		case StorageStatus::NotFound:
			return VaultError::NotSetup;
		case StorageStatus::Unavailable:
			return VaultError::BackendUnavailable;
		case StorageStatus::Error:
			break;
		}
		return VaultError::StorageError;
	}

} // namespace

const char *toString(VaultError error)
{
	switch (error) {
	case VaultError::None:
		return "success";
	case VaultError::InvalidArgument:
		return "invalid argument";
	case VaultError::CryptoInitFailed:
		return "failed to initialize libsodium";
	case VaultError::BackendUnavailable:
		return "storage backend is unavailable";
	case VaultError::StorageError:
		return "storage backend I/O error";
	case VaultError::NotSetup:
		return "vault is not set up";
	case VaultError::AlreadySetup:
		return "vault is already set up";
	case VaultError::Locked:
		return "vault is locked";
	case VaultError::WrongPin:
		return "wrong PIN or tampered EMK";
	case VaultError::CorruptedData:
		return "corrupted or unsupported data format";
	case VaultError::AuthenticationFailed:
		return "ciphertext authentication failed";
	case VaultError::KeyDerivationFailed:
		return "key derivation failed";
	case VaultError::MemoryLockFailed:
		return "failed to lock key memory";
	}
	return "unknown error";
}

Vault::Vault(ISecretStorageBackend *backend, std::string const &emkKey)
	: backend_(backend)
	, emkKey_(emkKey)
	, pendingKey_(emkKey + PENDING_SUFFIX)
{
}

Vault::~Vault()
{
	lock();
}

bool Vault::isAvailable() const
{
	return backend_ && backend_->isAvailable() && sodiumReady();
}

bool Vault::isUnlocked() const
{
	return unlocked_ && mk_.size() == MK_SIZE;
}

VaultState Vault::state() const
{
	if (!backend_) return VaultState::BackendUnavailable;
	if (isUnlocked()) return VaultState::Unlocked;
	Blob emk;
	StorageStatus status = backend_->load(emkKey_, &emk);
	if (status == StorageStatus::NotFound) {
		status = backend_->load(pendingKey_, &emk);
	}
	switch (status) {
	case StorageStatus::Ok:
		return VaultState::Locked;
	case StorageStatus::NotFound:
		return VaultState::NotSetup;
	case StorageStatus::Unavailable:
		return VaultState::BackendUnavailable;
	case StorageStatus::Error:
		break;
	}
	return VaultState::StorageError;
}

void Vault::lock()
{
	mk_.clear();
	unlocked_ = false;
}

VaultError Vault::deriveKek(SecureBuffer const &pin, Blob const &salt, uint32_t opsLimit, uint32_t memLimit, SecureBuffer *kek) const
{
	if (!kek) return VaultError::InvalidArgument;
	kek->clear();
	if (!validate_pin(pin)) return VaultError::InvalidArgument;
	if (salt.size() != SALT_SIZE
		|| opsLimit < MIN_ARGON2_OPSLIMIT || opsLimit > MAX_ARGON2_OPSLIMIT
		|| memLimit < MIN_ARGON2_MEMLIMIT || memLimit > MAX_ARGON2_MEMLIMIT) {
		return VaultError::CorruptedData;
	}

	SecureBuffer result(MK_SIZE);
	if (!result.lock()) return VaultError::MemoryLockFailed;
	if (crypto_pwhash(result.data(), result.size(), reinterpret_cast<const char *>(pin.data()), pin.size(), reinterpret_cast<unsigned char const *>(salt.data()), opsLimit, memLimit, crypto_pwhash_ALG_ARGON2ID13) != 0) {
		return VaultError::KeyDerivationFailed;
	}
	*kek = std::move(result);
	return VaultError::None;
}

VaultError Vault::createEmk(SecureBuffer const &mk, SecureBuffer const &pin, Blob *emk) const
{
	if (!emk) return VaultError::InvalidArgument;
	emk->clear();
	if (mk.size() != MK_SIZE) return VaultError::InvalidArgument;

	const Blob salt = generateRandom(SALT_SIZE);
	SecureBuffer kek;
	const VaultError err = deriveKek(pin, salt, ARGON2_OPSLIMIT, ARGON2_MEMLIMIT, &kek);
	if (err != VaultError::None) return err;

	Blob header;
	header.reserve(EMK_HEADER_SIZE);
	header.insert(header.end(), MAGIC, MAGIC + MAGIC_SIZE);
	header.push_back(static_cast<char>(VERSION));
	header.push_back(static_cast<char>(EMK_RECORD));
	header.push_back(static_cast<char>(XCHACHA20_POLY1305));
	header.push_back(static_cast<char>(ARGON2ID));
	appendU32(&header, ARGON2_OPSLIMIT);
	appendU32(&header, ARGON2_MEMLIMIT);
	header.push_back(static_cast<char>(SALT_SIZE));
	header.insert(header.end(), salt.begin(), salt.end());
	const Blob nonce = generateRandom(NONCE_SIZE);
	Blob encrypted(MK_SIZE + TAG_SIZE);
	unsigned long long encryptedLen = 0;
	if (crypto_aead_xchacha20poly1305_ietf_encrypt(reinterpret_cast<unsigned char *>(encrypted.data()),
			&encryptedLen, mk.data(),
			MK_SIZE,
			reinterpret_cast<unsigned char const *>(header.data()),
			header.size(),
			nullptr,
			reinterpret_cast<unsigned char const *>(nonce.data()),
			kek.data())
		!= 0) {
		return VaultError::CryptoInitFailed;
	}
	encrypted.resize(static_cast<size_t>(encryptedLen));
	emk->reserve(header.size() + nonce.size() + encrypted.size());
	emk->insert(emk->end(), header.begin(), header.end());
	emk->insert(emk->end(), nonce.begin(), nonce.end());
	emk->insert(emk->end(), encrypted.begin(), encrypted.end());
	return VaultError::None;
}

VaultError Vault::parseEmk(Blob const &emk, SecureBuffer const &pin, SecureBuffer *mk) const
{
	if (!mk) return VaultError::InvalidArgument;
	mk->clear();
	if (emk.size() != static_cast<size_t>(EMK_SIZE)
		|| !std::equal(emk.begin(), emk.begin() + MAGIC_SIZE, MAGIC)
		|| static_cast<uint8_t>(emk[MAGIC_SIZE]) != VERSION
		|| static_cast<uint8_t>(emk[MAGIC_SIZE + 1]) != EMK_RECORD
		|| static_cast<uint8_t>(emk[MAGIC_SIZE + 2]) != XCHACHA20_POLY1305
		|| static_cast<uint8_t>(emk[MAGIC_SIZE + 3]) != ARGON2ID
		|| static_cast<uint8_t>(emk[MAGIC_SIZE + 12]) != SALT_SIZE) {
		return VaultError::CorruptedData;
	}
	const Blob salt(emk.begin() + MAGIC_SIZE + 13, emk.begin() + MAGIC_SIZE + 13 + SALT_SIZE);
	SecureBuffer kek;
	const VaultError err = deriveKek(pin, salt, readU32(emk, MAGIC_SIZE + 4), readU32(emk, MAGIC_SIZE + 8), &kek);
	if (err != VaultError::None) return err;

	const Blob header(emk.begin(), emk.begin() + EMK_HEADER_SIZE);
	const Blob nonce(emk.begin() + EMK_HEADER_SIZE, emk.begin() + EMK_HEADER_SIZE + NONCE_SIZE);
	const Blob encrypted(emk.begin() + EMK_HEADER_SIZE + NONCE_SIZE, emk.end());
	SecureBuffer result(MK_SIZE);
	if (!result.lock()) return VaultError::MemoryLockFailed;
	unsigned long long resultLen = 0;
	const int rc = crypto_aead_xchacha20poly1305_ietf_decrypt(result.data(), &resultLen, nullptr,
		reinterpret_cast<unsigned char const *>(encrypted.data()), encrypted.size(),
		reinterpret_cast<unsigned char const *>(header.data()), header.size(),
		reinterpret_cast<unsigned char const *>(nonce.data()), kek.data());
	if (rc != 0 || resultLen != MK_SIZE) return VaultError::WrongPin;
	*mk = std::move(result);
	return VaultError::None;
}

VaultError Vault::storeVerified(std::string const &key, Blob const &data)
{
	const StorageStatus stored = backend_->store(key, data);
	if (stored != StorageStatus::Ok) {
		return stored == StorageStatus::Unavailable ? VaultError::BackendUnavailable : VaultError::StorageError;
	}
	Blob readBack;
	const StorageStatus loaded = backend_->load(key, &readBack);
	if (loaded == StorageStatus::Unavailable) return VaultError::BackendUnavailable;
	if (loaded != StorageStatus::Ok || readBack != data) return VaultError::StorageError;
	return VaultError::None;
}

VaultError Vault::writeEmk(Blob const &emk, bool createOnly)
{
	// 1. ステージングに書き込み・検証。失敗時は本キーに一切触れない
	VaultError err = storeVerified(pendingKey_, emk);
	if (err != VaultError::None) {
		backend_->remove(pendingKey_);
		return err;
	}
	if (createOnly) {
		// 新規作成時は本キーが存在しないことを書き込み直前に再確認する。
		// state() の時点ではキーリングのロック等で見えなかった既存 EMK を上書きしないため
		Blob existing;
		const StorageStatus status = backend_->load(emkKey_, &existing);
		if (status != StorageStatus::NotFound) {
			backend_->remove(pendingKey_);
			return status == StorageStatus::Ok ? VaultError::AlreadySetup : toVaultError(status);
		}
	}
	// 2. 本キーを置き換え。失敗時はステージングを復旧用に残す
	err = storeVerified(emkKey_, emk);
	if (err != VaultError::None) return err;
	// 3. 削除に失敗しても本キーと同内容なので次回 unlock 時に掃除される
	backend_->remove(pendingKey_);
	return VaultError::None;
}

VaultError Vault::openEmk(SecureBuffer const &pin, SecureBuffer *mk)
{
	if (!mk) return VaultError::InvalidArgument;
	mk->clear();
	Blob emk;
	VaultError mainError;
	const StorageStatus mainStatus = backend_->load(emkKey_, &emk);
	if (mainStatus == StorageStatus::Ok) {
		mainError = parseEmk(emk, pin, mk);
		if (mainError == VaultError::None) {
			// 中断された書き換えの残骸を掃除する
			backend_->remove(pendingKey_);
			return VaultError::None;
		}
		if (mainError != VaultError::WrongPin && mainError != VaultError::CorruptedData) return mainError;
	} else if (mainStatus == StorageStatus::NotFound) {
		mainError = VaultError::NotSetup;
	} else {
		return toVaultError(mainStatus);
	}

	// 本キーで開けない場合、中断された書き換えのステージングを試す
	Blob pending;
	const StorageStatus pendingStatus = backend_->load(pendingKey_, &pending);
	if (pendingStatus == StorageStatus::NotFound) return mainError;
	if (pendingStatus != StorageStatus::Ok) {
		return mainError == VaultError::NotSetup ? toVaultError(pendingStatus) : mainError;
	}
	const VaultError pendingError = parseEmk(pending, pin, mk);
	if (pendingError != VaultError::None) {
		return mainError == VaultError::NotSetup ? pendingError : mainError;
	}
	// ステージングを本キーへ昇格。失敗してもステージングが残るので次回再試行される
	if (storeVerified(emkKey_, pending) == VaultError::None) {
		backend_->remove(pendingKey_);
	}
	return VaultError::None;
}

VaultError Vault::newMasterKey(SecureBuffer *mk)
{
	*mk = { };
	SecureBuffer master(MK_SIZE);
	if (!master.lock()) return VaultError::MemoryLockFailed;
	randombytes_buf(master.data(), master.size());
	*mk = std::move(master);
	return VaultError::None;
}

VaultError Vault::setup(SecureBuffer const &pin)
{
	if (!validate_pin(pin)) return VaultError::InvalidArgument;
	if (!backend_) return VaultError::BackendUnavailable;
	if (!sodiumReady()) return VaultError::CryptoInitFailed;
	switch (state()) {
	case VaultState::NotSetup:
		break;
	case VaultState::Locked:
	case VaultState::Unlocked:
		return VaultError::AlreadySetup;
	case VaultState::BackendUnavailable:
		return VaultError::BackendUnavailable;
	case VaultState::StorageError:
		return VaultError::StorageError;
	}

	SecureBuffer master;
	VaultError err = newMasterKey(&master);
	if (err != VaultError::None) return err;
	Blob emk;
	err = createEmk(master, pin, &emk);
	if (err != VaultError::None) return err;
	err = writeEmk(emk, true);
	if (err != VaultError::None) return err;
	mk_ = std::move(master);
	unlocked_ = true;
	return VaultError::None;
}

VaultError Vault::unlock(SecureBuffer const &pin)
{
	if (!validate_pin(pin)) return VaultError::InvalidArgument;
	if (!backend_) return VaultError::BackendUnavailable;
	if (!sodiumReady()) return VaultError::CryptoInitFailed;
	SecureBuffer master;
	const VaultError err = openEmk(pin, &master);
	if (err != VaultError::None) return err;
	lock();
	mk_ = std::move(master);
	unlocked_ = true;
	return VaultError::None;
}

VaultError Vault::encrypt(SecureBuffer const &plain, Blob *cipher)
{
	return encryptBytes(plain.data(), plain.size(), cipher);
}

VaultError Vault::encrypt(Blob const &plain, Blob *cipher)
{
	return encryptBytes(reinterpret_cast<unsigned char const *>(plain.data()), plain.size(), cipher);
}

VaultError Vault::encryptBytes(unsigned char const *plain, size_t size, Blob *cipher)
{
	*cipher = { };
	if (!isUnlocked()) return VaultError::Locked;
	Blob header;
	header.reserve(DATA_HEADER_SIZE);
	header.insert(header.end(), MAGIC, MAGIC + MAGIC_SIZE);
	header.push_back(static_cast<char>(VERSION));
	header.push_back(static_cast<char>(DATA_RECORD));
	header.push_back(static_cast<char>(XCHACHA20_POLY1305));
	const Blob nonce = generateRandom(NONCE_SIZE);
	Blob encrypted(size + TAG_SIZE);
	unsigned long long encryptedLen = 0;
	if (crypto_aead_xchacha20poly1305_ietf_encrypt(reinterpret_cast<unsigned char *>(encrypted.data()), &encryptedLen,
			plain, size,
			reinterpret_cast<unsigned char const *>(header.data()), header.size(), nullptr,
			reinterpret_cast<unsigned char const *>(nonce.data()), mk_.data())
		!= 0) {
		return VaultError::InvalidArgument;
	}
	encrypted.resize(static_cast<size_t>(encryptedLen));
	cipher->reserve(header.size() + nonce.size() + encrypted.size());
	cipher->insert(cipher->end(), header.begin(), header.end());
	cipher->insert(cipher->end(), nonce.begin(), nonce.end());
	cipher->insert(cipher->end(), encrypted.begin(), encrypted.end());
	return VaultError::None;
}

VaultError Vault::decryptToSecureBuffer(Blob const &cipher, SecureBuffer *plain)
{
	plain->clear();
	if (!isUnlocked()) return VaultError::Locked;
	if (cipher.size() < static_cast<size_t>(DATA_HEADER_SIZE + NONCE_SIZE + TAG_SIZE)
		|| !std::equal(cipher.begin(), cipher.begin() + MAGIC_SIZE, MAGIC)
		|| static_cast<uint8_t>(cipher[MAGIC_SIZE]) != VERSION
		|| static_cast<uint8_t>(cipher[MAGIC_SIZE + 1]) != DATA_RECORD
		|| static_cast<uint8_t>(cipher[MAGIC_SIZE + 2]) != XCHACHA20_POLY1305) {
		return VaultError::CorruptedData;
	}
	const Blob header(cipher.begin(), cipher.begin() + DATA_HEADER_SIZE);
	const Blob nonce(cipher.begin() + DATA_HEADER_SIZE, cipher.begin() + DATA_HEADER_SIZE + NONCE_SIZE);
	const Blob encrypted(cipher.begin() + DATA_HEADER_SIZE + NONCE_SIZE, cipher.end());
	SecureBuffer result(encrypted.size() - TAG_SIZE);
	// 空の平文はロック対象がない
	if (!result.empty() && !result.lock()) return VaultError::MemoryLockFailed;
	unsigned long long resultLen = 0;
	if (crypto_aead_xchacha20poly1305_ietf_decrypt(result.data(), &resultLen, nullptr,
			reinterpret_cast<unsigned char const *>(encrypted.data()), encrypted.size(),
			reinterpret_cast<unsigned char const *>(header.data()), header.size(),
			reinterpret_cast<unsigned char const *>(nonce.data()), mk_.data())
			!= 0
		|| resultLen != result.size()) {
		return VaultError::AuthenticationFailed;
	}
	*plain = std::move(result);
	return VaultError::None;
}

VaultError Vault::changePin(SecureBuffer const &oldPin, SecureBuffer const &newPin)
{
	if (!validate_pin(oldPin)) return VaultError::InvalidArgument;
	if (!validate_pin(newPin)) return VaultError::InvalidArgument;
	if (!isUnlocked()) return VaultError::Locked;
	SecureBuffer verifiedMaster;
	VaultError err = openEmk(oldPin, &verifiedMaster);
	if (err != VaultError::None) return err;
	// 保存されている EMK がメモリ上の MK と別物なら、誤って別の鍵を包み直さない
	if (sodium_memcmp(verifiedMaster.data(), mk_.data(), MK_SIZE) != 0) return VaultError::CorruptedData;
	Blob newEmk;
	err = createEmk(verifiedMaster, newPin, &newEmk);
	if (err != VaultError::None) return err;
	return writeEmk(newEmk, false);
}

VaultError Vault::reset(SecureBuffer const &newPin)
{
	if (!validate_pin(newPin)) return VaultError::InvalidArgument;
	if (!backend_) return VaultError::BackendUnavailable;
	if (!sodiumReady()) return VaultError::CryptoInitFailed;

	// 新しい EMK の書き込みに成功するまで既存 EMK は残す
	SecureBuffer master;
	VaultError err = newMasterKey(&master);
	if (err != VaultError::None) return err;
	Blob emk;
	err = createEmk(master, newPin, &emk);
	if (err != VaultError::None) return err;
	err = writeEmk(emk, false);
	if (err != VaultError::None) return err;
	lock();
	mk_ = std::move(master);
	unlocked_ = true;
	return VaultError::None;
}

VaultError Vault::destroy()
{
	if (!backend_) return VaultError::BackendUnavailable;
	lock();
	// 本キーだけ消えてステージングが残ると古い EMK が復活するため、ステージングを先に消す
	for (const std::string *key : { &pendingKey_, &emkKey_ }) {
		const StorageStatus status = backend_->remove(*key);
		if (status != StorageStatus::Ok && status != StorageStatus::NotFound) return toVaultError(status);
	}
	return VaultError::None;
}

} // namespace localvault
