#include <QApplication>
#include <QByteArray>
#include <QCoreApplication>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <map>
#include <memory>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/stat.h>
#endif

#include "app/BackendSelector.h"
#include "storage/FileBackend.h"
#include "storage/SystemKeychainBackend.h"
#include "vault/ProcessHardening.h"
#include "vault/SecureBuffer.h"
#include "vault/Vault.h"
#include "gui/SetupVaultDialog.h"
#include "gui/ResetVaultDialog.h"
#include "gui/UnlockVaultDialog.h"

using namespace localvault;

static const char EMK_KEY[] = "vault_emk";

// ===== Qt / std conversion helpers =====

static Blob QByteArrayToBlob(const QByteArray &ba)
{
	return Blob(ba.constData(), ba.constData() + ba.size());
}

static std::string QStringToStd(const QString &s)
{
	return s.toUtf8().toStdString();
}

static QString stdToQString(std::string const &s)
{
	return QString::fromUtf8(s.c_str(), static_cast<int>(s.size()));
}

static std::filesystem::path QStringToPath(const QString &s)
{
#if defined(_WIN32)
	// Windows では narrow 文字列が ANSI コードページとして解釈されるため、UTF-16 で渡す
	return std::filesystem::path(s.toStdWString());
#else
	return std::filesystem::path(QStringToStd(s));
#endif
}

static QString pathToQString(const std::filesystem::path &p)
{
#if defined(_WIN32)
	return QString::fromStdWString(p.wstring());
#else
	return stdToQString(p.string());
#endif
}

class LocalVaultTest {
public:
	// ===== Test harness =====

	int tests_run = 0;
	int tests_passed = 0;

	void check(bool condition, const char *name)
	{
		tests_run++;
		if (condition) {
			tests_passed++;
			std::printf("PASS: %s\n", name);
		} else {
			std::printf("FAIL: %s\n", name);
		}
	}

	static SecureBuffer pinFromUtf8(const char *s)
	{
		SecureBuffer pin;
		pin.assign(s, strlen(s));
		return pin;
	}

	static bool secureBufferEquals(SecureBuffer const &buf, Blob const &blob)
	{
		return buf.size() == blob.size()
			&& std::equal(buf.begin(), buf.end(), blob.begin(), blob.end());
	}

	static bool blobEquals(Blob const &a, const QByteArray &b)
	{
		return a.size() == static_cast<size_t>(b.size())
			&& std::equal(a.begin(), a.end(), b.begin(), b.end());
	}

	static Blob encryptOrEmpty(Vault *vault, Blob const &plain)
	{
		Blob cipher;
		if (!vault) return cipher;
		(void)vault->encrypt(plain, &cipher);
		return cipher;
	}

	static bool decryptsTo(Vault *vault, Blob const &cipher, Blob const &expected)
	{
		if (!vault) return false;
		SecureBuffer plain;
		return vault->decryptToSecureBuffer(cipher, &plain) == VaultError::None && secureBufferEquals(plain, expected);
	}

	/**
	 * @brief 障害注入用のインメモリバックエンド
	 *
	 * 状態は共有されるため、BackendSelector のファクトリから複数回生成しても同じ内容を参照する。
	 */
	class MemoryBackend : public ISecretStorageBackend {
	public:
		struct State {
			std::map<std::string, Blob> items;
			bool available = true;
			std::string failStoreKey; ///< この key への store は Error を返す
			std::string hiddenKey; ///< この key の load を hiddenLoads 回だけ NotFound にする
			int hiddenLoads = 0;
		};

		explicit MemoryBackend(std::shared_ptr<State> state = std::make_shared<State>())
			: state_(std::move(state))
		{
		}

		State &state() { return *state_; }

		bool isAvailable() const override { return state_->available; }
		std::string name() const override { return "memory"; }

		StorageStatus store(std::string const &key, Blob const &data) override
		{
			if (!state_->available) return StorageStatus::Unavailable;
			if (key == state_->failStoreKey) return StorageStatus::Error;
			state_->items[key] = data;
			return StorageStatus::Ok;
		}

		StorageStatus load(std::string const &key, Blob *out) override
		{
			if (!out) return StorageStatus::Error;
			out->clear();
			if (!state_->available) return StorageStatus::Unavailable;
			if (key == state_->hiddenKey && state_->hiddenLoads > 0) {
				--state_->hiddenLoads;
				return StorageStatus::NotFound;
			}
			auto it = state_->items.find(key);
			if (it == state_->items.end()) return StorageStatus::NotFound;
			*out = it->second;
			return StorageStatus::Ok;
		}

		StorageStatus remove(std::string const &key) override
		{
			if (!state_->available) return StorageStatus::Unavailable;
			return state_->items.erase(key) ? StorageStatus::Ok : StorageStatus::NotFound;
		}

	private:
		std::shared_ptr<State> state_;
	};

	void testSecureBuffer()
	{
		SecureBuffer buf(32);
		check(buf.size() == 32, "SecureBuffer size");

		std::fill(buf.begin(), buf.end(), 0xab);
		check(buf.lock(), "SecureBuffer locks memory");
		check(!buf.resize(64), "SecureBuffer rejects resize while locked");
		buf.clear();
		check(buf.empty(), "SecureBuffer empty after clear");

		// 再確保を伴う拡張でも内容が保持されること（旧領域はアロケータがゼロクリアする）
		SecureBuffer grow;
		bool ok = true;
		for (int i = 0; i < 1000; ++i) {
			const uint8_t b = static_cast<uint8_t>(i);
			ok = grow.append(&b, 1) && ok;
		}
		for (int i = 0; i < 1000; ++i) {
			ok = ok && grow[i] == static_cast<uint8_t>(i);
		}
		check(ok && grow.size() == 1000, "SecureBuffer append across reallocations");
		check(grow.resize(5000) && grow[999] == static_cast<uint8_t>(999), "SecureBuffer resize grow keeps data");

		SecureBuffer moved(std::move(grow));
		check(moved.size() == 5000 && grow.empty(), "SecureBuffer move");

		SecureBuffer a = pinFromUtf8("abc");
		check(a.equals(pinFromUtf8("abc")) && !a.equals(pinFromUtf8("abd")) && !a.equals(pinFromUtf8("ab")), "SecureBuffer equals");
		check(SecureBuffer().equals(SecureBuffer()), "SecureBuffer equals empty");
	}

	void testFileBackend()
	{
		QTemporaryDir tempDir;
		check(tempDir.isValid(), "FileBackend temp dir valid");

		FileBackend backend(QStringToPath(tempDir.path()));
		Blob out;
		check(backend.load("missing", &out) == StorageStatus::NotFound && out.empty(), "FileBackend load missing returns NotFound");
		check(backend.remove("missing") == StorageStatus::NotFound, "FileBackend remove missing returns NotFound");

		check(backend.store("item", QByteArrayToBlob("first")) == StorageStatus::Ok, "FileBackend store");
		check(backend.store("item", QByteArrayToBlob("second")) == StorageStatus::Ok, "FileBackend overwrite");
		check(backend.load("item", &out) == StorageStatus::Ok && blobEquals(out, "second"), "FileBackend load returns latest");

#if defined(__unix__) || defined(__APPLE__)
		struct stat st { };
		const std::string filePath = QStringToStd(tempDir.path()) + "/item";
		check(::stat(filePath.c_str(), &st) == 0 && (st.st_mode & 0777) == 0600, "FileBackend file mode is 0600");
		check(std::distance(std::filesystem::directory_iterator(QStringToPath(tempDir.path())), std::filesystem::directory_iterator()) == 1,
			"FileBackend leaves no temp files");
#endif

		check(backend.remove("item") == StorageStatus::Ok, "FileBackend remove");
		check(backend.load("item", &out) == StorageStatus::NotFound, "FileBackend removed");

		// 無害化が必要なキーで保存・読み出し
		const std::string dangerousKey = "../<bad>:key|name?*";
		check(backend.store(dangerousKey, QByteArrayToBlob("test data")) == StorageStatus::Ok, "FileBackend stores sanitized key");
		check(backend.load(dangerousKey, &out) == StorageStatus::Ok && blobEquals(out, "test data"), "FileBackend loads sanitized key");
		check(backend.remove(dangerousKey) == StorageStatus::Ok, "FileBackend removes sanitized key");
	}

	void testVaultSetupUnlock()
	{
		QTemporaryDir tempDir;
		check(tempDir.isValid(), "Vault temp dir valid");

		FileBackend backend(QStringToPath(tempDir.path()));
		Vault vault(&backend);

		check(vault.isAvailable(), "Vault backend available");
		check(vault.state() == VaultState::NotSetup, "Vault not setup initially");
		check(!vault.isUnlocked(), "Vault not unlocked initially");

		SecureBuffer pin = pinFromUtf8("1234");
		check(vault.setup(pin) == VaultError::None, "Vault setup");
		check(vault.state() == VaultState::Unlocked, "Vault is unlocked after setup");

		Blob secret = QByteArrayToBlob("API_KEY=sk-1234567890abcdef");
		Blob encrypted = encryptOrEmpty(&vault, secret);
		check(!encrypted.empty(), "Vault encrypt");
		check(encrypted != secret, "Vault encryption changes data");
		check(decryptsTo(&vault, encrypted, secret), "Vault decrypt");
		check(vault.encrypt(secret, nullptr) == VaultError::InvalidArgument, "Vault rejects null cipher output");
		check(vault.decryptToSecureBuffer(encrypted, nullptr) == VaultError::InvalidArgument, "Vault rejects null plain output");

		Blob emptyCipher = encryptOrEmpty(&vault, Blob());
		check(!emptyCipher.empty() && decryptsTo(&vault, emptyCipher, Blob()), "Vault empty plaintext roundtrip");

		SecureBuffer securePlain(secret.data(), secret.size());
		Blob secureCipher;
		check(vault.encrypt(securePlain, &secureCipher) == VaultError::None && decryptsTo(&vault, secureCipher, secret), "Vault encrypt from SecureBuffer");

		vault.lock();
		check(vault.state() == VaultState::Locked, "Vault locked state");
		SecureBuffer plain;
		check(vault.decryptToSecureBuffer(encrypted, &plain) == VaultError::Locked, "Vault decrypt fails with Locked");
		Blob cipher;
		check(vault.encrypt(secret, &cipher) == VaultError::Locked && cipher.empty(), "Vault encrypt fails with Locked");

		check(vault.unlock(pin) == VaultError::None, "Vault unlock");
		check(decryptsTo(&vault, encrypted, secret), "Vault decrypt after unlock");
	}

	void testVaultWrongPin()
	{
		QTemporaryDir tempDir;
		FileBackend backend(QStringToPath(tempDir.path()));
		Vault vault(&backend);

		SecureBuffer pin = pinFromUtf8("correct");
		check(vault.setup(pin) == VaultError::None, "Vault setup for wrong PIN test");
		check(vault.setup(pin) == VaultError::AlreadySetup, "Vault refuses to overwrite existing setup");
		vault.lock();
		check(vault.setup(pin) == VaultError::AlreadySetup, "Vault refuses to overwrite existing setup while locked");

		check(vault.unlock(pinFromUtf8("wrong")) == VaultError::WrongPin, "Vault unlock with wrong PIN returns WrongPin");
		if (allow_empty_pin) {
			check(vault.unlock(SecureBuffer()) == VaultError::WrongPin, "Vault unlock with empty PIN returns WrongPin when empty PIN is allowed");
		} else {
			check(vault.unlock(SecureBuffer()) == VaultError::InvalidArgument, "Vault unlock with empty PIN returns InvalidArgument");
		}
	}

	void testVaultChangePin()
	{
		QTemporaryDir tempDir;
		FileBackend backend(QStringToPath(tempDir.path()));
		Vault vault(&backend);

		SecureBuffer oldPin = pinFromUtf8("oldpin");
		SecureBuffer newPin = pinFromUtf8("newpin");

		check(vault.setup(oldPin) == VaultError::None, "Vault setup for PIN change");

		Blob secret = QByteArrayToBlob("SuperSecretToken");
		Blob encrypted = encryptOrEmpty(&vault, secret);

		check(vault.changePin(newPin, newPin) == VaultError::WrongPin, "Vault change PIN with wrong old PIN fails");
		check(vault.changePin(oldPin, newPin) == VaultError::None, "Vault change PIN");
		check(vault.isUnlocked(), "Vault still unlocked after PIN change");

		vault.lock();
		check(vault.unlock(oldPin) == VaultError::WrongPin, "Vault unlock with old PIN fails");
		check(vault.unlock(newPin) == VaultError::None, "Vault unlock with new PIN succeeds");
		check(decryptsTo(&vault, encrypted, secret), "Vault decrypt after PIN change");
	}

	void testVaultChangePinStorageFailure()
	{
		SecureBuffer oldPin = pinFromUtf8("oldpin");
		SecureBuffer newPin = pinFromUtf8("newpin");
		const std::string pendingKey = std::string(EMK_KEY) + ".pending";

		// 本キーの置き換えに失敗 → 新 PIN で解除するとステージングが昇格する
		{
			MemoryBackend backend;
			Vault vault(&backend, EMK_KEY);
			check(vault.setup(oldPin) == VaultError::None, "Vault setup for main-store failure");
			Blob secret = QByteArrayToBlob("secret");
			Blob encrypted = encryptOrEmpty(&vault, secret);

			backend.state().failStoreKey = EMK_KEY;
			check(vault.changePin(oldPin, newPin) == VaultError::StorageError, "Vault change PIN reports StorageError");
			check(backend.state().items.count(EMK_KEY) == 1, "Vault keeps old EMK after failed change");
			check(backend.state().items.count(pendingKey) == 1, "Vault keeps pending EMK after failed change");

			backend.state().failStoreKey.clear();
			vault.lock();
			check(vault.unlock(newPin) == VaultError::None, "Vault unlock with new PIN via pending EMK");
			check(backend.state().items.count(pendingKey) == 0, "Vault promotes pending EMK");
			check(decryptsTo(&vault, encrypted, secret), "Vault decrypt after pending promotion");
			vault.lock();
			check(vault.unlock(oldPin) == VaultError::WrongPin, "Vault old PIN fails after promotion");
		}

		// 本キーの置き換えに失敗 → 旧 PIN で解除すると変更は破棄される
		{
			MemoryBackend backend;
			Vault vault(&backend, EMK_KEY);
			check(vault.setup(oldPin) == VaultError::None, "Vault setup for abandoned change");
			backend.state().failStoreKey = EMK_KEY;
			check(vault.changePin(oldPin, newPin) == VaultError::StorageError, "Vault change PIN fails for abandoned change");
			backend.state().failStoreKey.clear();
			vault.lock();
			check(vault.unlock(oldPin) == VaultError::None, "Vault unlock with old PIN after failed change");
			check(backend.state().items.count(pendingKey) == 0, "Vault discards pending EMK after old PIN unlock");
			vault.lock();
			check(vault.unlock(newPin) == VaultError::WrongPin, "Vault new PIN fails after discarding pending EMK");
		}

		// ステージングの書き込みに失敗 → 本キーは一切変更されない
		{
			MemoryBackend backend;
			Vault vault(&backend, EMK_KEY);
			check(vault.setup(oldPin) == VaultError::None, "Vault setup for pending-store failure");
			const Blob before = backend.state().items[EMK_KEY];
			backend.state().failStoreKey = pendingKey;
			check(vault.changePin(oldPin, newPin) == VaultError::StorageError, "Vault change PIN fails when pending store fails");
			check(backend.state().items[EMK_KEY] == before, "Vault main EMK untouched when pending store fails");
		}
	}

	void testVaultReset()
	{
		QTemporaryDir tempDir;
		FileBackend backend(QStringToPath(tempDir.path()));
		Vault vault(&backend);

		SecureBuffer oldPin = pinFromUtf8("oldpin");
		SecureBuffer newPin = pinFromUtf8("newpin");
		check(vault.setup(oldPin) == VaultError::None, "Vault setup for reset");
		Blob oldCipher = encryptOrEmpty(&vault, QByteArrayToBlob("OldSecret"));

		check(vault.reset(newPin) == VaultError::None, "Vault reset");
		check(vault.isUnlocked(), "Vault unlocked after reset");
		SecureBuffer plain;
		check(vault.decryptToSecureBuffer(oldCipher, &plain) == VaultError::AuthenticationFailed, "Vault cannot decrypt pre-reset data");

		vault.lock();
		check(vault.unlock(oldPin) == VaultError::WrongPin, "Vault old PIN fails after reset");
		check(vault.unlock(newPin) == VaultError::None, "Vault new PIN succeeds after reset");
	}

	void testVaultResetFailureKeepsOld()
	{
		MemoryBackend backend;
		Vault vault(&backend, EMK_KEY);
		SecureBuffer oldPin = pinFromUtf8("oldpin");
		check(vault.setup(oldPin) == VaultError::None, "Vault setup for failed reset");
		Blob secret = QByteArrayToBlob("keep me");
		Blob encrypted = encryptOrEmpty(&vault, secret);

		backend.state().failStoreKey = std::string(EMK_KEY) + ".pending";
		check(vault.reset(pinFromUtf8("newpin")) == VaultError::StorageError, "Vault reset reports StorageError");
		check(decryptsTo(&vault, encrypted, secret), "Vault keeps old MK in memory after failed reset");
		backend.state().failStoreKey.clear();
		vault.lock();
		check(vault.unlock(oldPin) == VaultError::None, "Vault old PIN still works after failed reset");
	}

	void testVaultErrors()
	{
		MemoryBackend backend;
		Vault vault(&backend, EMK_KEY);
		SecureBuffer pin = pinFromUtf8("1234");

		backend.state().available = false;
		check(vault.state() == VaultState::BackendUnavailable, "Vault state reports BackendUnavailable");
		check(vault.setup(pin) == VaultError::BackendUnavailable, "Vault setup does not proceed when backend unavailable");
		check(vault.unlock(pin) == VaultError::BackendUnavailable, "Vault unlock reports BackendUnavailable");
		backend.state().available = true;

		check(vault.unlock(pin) == VaultError::NotSetup, "Vault unlock reports NotSetup");
		check(vault.setup(pin) == VaultError::None, "Vault setup for error tests");
		Blob encrypted = encryptOrEmpty(&vault, QByteArrayToBlob("TamperTestSecret"));

		SecureBuffer plain;
		Blob tampered = encrypted;
		tampered[tampered.size() / 2] ^= 0xff;
		check(vault.decryptToSecureBuffer(tampered, &plain) == VaultError::AuthenticationFailed, "Vault detects tampered ciphertext");
		tampered = encrypted;
		tampered[4] = 99;
		check(vault.decryptToSecureBuffer(tampered, &plain) == VaultError::CorruptedData, "Vault rejects unsupported data version");
		check(vault.decryptToSecureBuffer(Blob(10), &plain) == VaultError::CorruptedData, "Vault rejects truncated ciphertext");

		vault.lock();
		const Blob emk = backend.state().items[EMK_KEY];
		Blob badEmk = emk;
		badEmk.back() ^= 0x01;
		backend.state().items[EMK_KEY] = badEmk;
		check(vault.unlock(pin) == VaultError::WrongPin, "Vault tampered EMK body returns WrongPin");
		badEmk = emk;
		badEmk.pop_back();
		backend.state().items[EMK_KEY] = badEmk;
		check(vault.unlock(pin) == VaultError::CorruptedData, "Vault truncated EMK returns CorruptedData");
		badEmk = emk;
		badEmk[12] = static_cast<char>(0x7f); // memlimit（オフセット 12、ビッグエンディアン）を上限超過に改ざん
		backend.state().items[EMK_KEY] = badEmk;
		check(vault.unlock(pin) == VaultError::CorruptedData, "Vault out-of-range Argon2 params return CorruptedData");
		backend.state().items[EMK_KEY] = emk;
		check(vault.unlock(pin) == VaultError::None, "Vault unlock after restoring EMK");
	}

	void testVaultSetupDoesNotOverwriteHiddenEmk()
	{
		// キーリングのロック等で state() 時点では EMK が見えず、書き込み直前に見えるようになるケース
		MemoryBackend backend;
		Vault original(&backend, EMK_KEY);
		check(original.setup(pinFromUtf8("original")) == VaultError::None, "Vault setup original for hidden EMK test");
		const Blob before = backend.state().items[EMK_KEY];

		Vault vault(&backend, EMK_KEY);
		backend.state().hiddenKey = EMK_KEY;
		backend.state().hiddenLoads = 1;
		check(vault.setup(pinFromUtf8("other")) == VaultError::AlreadySetup, "Vault setup detects EMK right before writing");
		check(backend.state().items[EMK_KEY] == before, "Vault setup leaves existing EMK untouched");
		check(backend.state().items.count(std::string(EMK_KEY) + ".pending") == 0, "Vault setup removes pending EMK after abort");
	}

	void testVaultDestroy()
	{
		MemoryBackend backend;
		Vault vault(&backend, EMK_KEY);
		check(vault.setup(pinFromUtf8("1234")) == VaultError::None, "Vault setup for destroy");
		backend.state().items[std::string(EMK_KEY) + ".pending"] = Blob(3);
		check(vault.destroy() == VaultError::None, "Vault destroy");
		check(!vault.isUnlocked() && backend.state().items.empty(), "Vault destroy removes EMK and pending EMK");
		check(vault.state() == VaultState::NotSetup, "Vault not setup after destroy");
	}

	void testBackendSelector()
	{
		QTemporaryDir tempDir;
		const std::filesystem::path dir = QStringToPath(tempDir.path());
		auto systemState = std::make_shared<MemoryBackend::State>();
		BackendSelector selector(dir, "com.example.localvault.Vault", EMK_KEY, [systemState] { return std::make_unique<MemoryBackend>(systemState); });
		using Status = BackendSelection::Status;

		systemState->available = false;
		BackendSelection sel = selector.select(false);
		check(sel.status == Status::NeedsFileConsent && sel.kind == BackendKind::File && sel.backend, "BackendSelector requires consent for file fallback");

		systemState->available = true;
		sel = selector.select(false);
		check(sel.status == Status::Ok && sel.kind == BackendKind::System && !sel.recorded, "BackendSelector proposes system backend");
		check(selector.record(BackendKind::System) == StorageStatus::Ok, "BackendSelector records backend");

		// 記録済みの保存先が使えない場合、既存のファイル Vault があっても切り替えない
		FileBackend(dir / "emk").store(EMK_KEY, Blob(1));
		systemState->available = false;
		sel = selector.select(false);
		check(sel.status == Status::RecordedBackendUnavailable && sel.kind == BackendKind::System && !sel.backend,
			"BackendSelector does not fall back when recorded backend unavailable");

		// 記録なし + ファイルに既存 Vault（旧バージョン）→ ファイルを採用して記録
		check(selector.forget() == StorageStatus::Ok, "BackendSelector forget");
		systemState->available = true;
		sel = selector.select(false);
		check(sel.status == Status::Ok && sel.kind == BackendKind::File && sel.recorded, "BackendSelector migrates legacy file vault");
		systemState->available = false;
		sel = selector.select(false);
		check(sel.status == Status::Ok && sel.kind == BackendKind::File, "BackendSelector uses recorded file backend");

		// 記録なし + システムに既存 Vault → システムを優先
		selector.forget();
		systemState->available = true;
		systemState->items[EMK_KEY] = Blob(1);
		sel = selector.select(false);
		check(sel.status == Status::Ok && sel.kind == BackendKind::System && sel.recorded, "BackendSelector prefers existing system vault");

		FileBackend(dir).store("storage-backend", QByteArrayToBlob("bogus\n"));
		check(selector.select(false).status == Status::ConfigError, "BackendSelector rejects invalid record");
	}

	void testSystemKeychainBackend()
	{
		SystemKeychainBackend backend;
		if (!backend.isAvailable()) {
			std::printf("SKIP: SystemKeychainBackend not available\n");
			return;
		}

		const std::string testKey = "localvault-phase3-test";
		backend.remove(testKey);
		Blob loaded;
		check(backend.load(testKey, &loaded) == StorageStatus::NotFound, "SystemKeychainBackend load missing returns NotFound");

		check(backend.store(testKey, QByteArrayToBlob("first")) == StorageStatus::Ok, "SystemKeychainBackend store");
		check(backend.store(testKey, QByteArrayToBlob("EMK blob for system keychain test")) == StorageStatus::Ok, "SystemKeychainBackend overwrite");
		check(backend.load(testKey, &loaded) == StorageStatus::Ok && blobEquals(loaded, "EMK blob for system keychain test"), "SystemKeychainBackend load");
		check(backend.remove(testKey) == StorageStatus::Ok, "SystemKeychainBackend remove");
		check(backend.load(testKey, &loaded) == StorageStatus::NotFound, "SystemKeychainBackend removed");
		check(backend.remove(testKey) == StorageStatus::NotFound, "SystemKeychainBackend remove missing returns NotFound");
	}

	void testVaultWithSystemKeychain()
	{
		SystemKeychainBackend backend;
		if (!backend.isAvailable()) {
			std::printf("SKIP: SystemKeychainBackend not available for Vault test\n");
			return;
		}

		const std::string testKey = "localvault-phase3-vault-test";
		Vault vault(&backend, testKey);
		(void)vault.destroy();

		SecureBuffer pin = pinFromUtf8("phase3pin");
		check(vault.setup(pin) == VaultError::None, "Vault setup with SystemKeychainBackend");

		Blob secret = QByteArrayToBlob("Secret stored via system keychain");
		Blob encrypted = encryptOrEmpty(&vault, secret);
		check(!encrypted.empty(), "Vault encrypt with SystemKeychainBackend");

		vault.lock();
		check(vault.unlock(pin) == VaultError::None, "Vault unlock with SystemKeychainBackend");
		check(decryptsTo(&vault, encrypted, secret), "Vault decrypt with SystemKeychainBackend");

		check(vault.destroy() == VaultError::None, "Vault destroy with SystemKeychainBackend");
	}

	int runTests()
	{

		std::printf("=== Secure Memory Tests ===\n");
		testSecureBuffer();

		std::printf("\n=== Vault Core Tests ===\n");
		testFileBackend();
		testVaultSetupUnlock();
		testVaultWrongPin();
		testVaultChangePin();
		testVaultChangePinStorageFailure();
		testVaultReset();
		testVaultResetFailureKeepsOld();
		testVaultErrors();
		testVaultSetupDoesNotOverwriteHiddenEmk();
		testVaultDestroy();
		testBackendSelector();

		std::printf("\n=== System Key Chain Backend Tests ===\n");
		testSystemKeychainBackend();
		testVaultWithSystemKeychain();

		std::printf("\nResults: %d / %d passed\n", tests_passed, tests_run);

		return (tests_passed == tests_run) ? 0 : 1;
	}
};

// ===== GUI demo =====

static std::filesystem::path appConfigDirectory()
{
	return QStringToPath(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
}

static QString vaultErrorMessage(VaultError error)
{
	switch (error) {
	case VaultError::None:
		return QObject::tr("Success.");
	case VaultError::WrongPin:
		return QObject::tr("Incorrect PIN.");
	case VaultError::BackendUnavailable:
		return QObject::tr("The Vault storage is not available. If you use a system keyring, make sure it is running and unlocked, then try again.");
	case VaultError::StorageError:
		return QObject::tr("Failed to read or write the Vault storage. If you were changing the PIN, either the old or the new PIN may be in effect.");
	case VaultError::CorruptedData:
		return QObject::tr("The Vault data is corrupted or uses an unsupported format.");
	case VaultError::AuthenticationFailed:
		return QObject::tr("The encrypted data was tampered with or was encrypted with a different Vault key.");
	case VaultError::KeyDerivationFailed:
		return QObject::tr("Failed to derive the key from the PIN. The system may be low on memory.");
	case VaultError::MemoryLockFailed:
		return QObject::tr("Failed to lock memory for key material. The memory lock limit (RLIMIT_MEMLOCK) may be too low.");
	case VaultError::NotSetup:
		return QObject::tr("The Vault has not been set up.");
	case VaultError::AlreadySetup:
		return QObject::tr("A Vault already exists.");
	case VaultError::Locked:
		return QObject::tr("The Vault is locked.");
	case VaultError::InvalidArgument:
		return QObject::tr("Invalid input.");
	case VaultError::CryptoInitFailed:
		return QObject::tr("Failed to initialize the cryptographic library.");
	}
	return QObject::tr("Unknown error.");
}

static bool askRetry(const QString &title, const QString &message)
{
	return QMessageBox::critical(nullptr, title, message, QMessageBox::Retry | QMessageBox::Cancel, QMessageBox::Retry) == QMessageBox::Retry;
}

/**
 * @brief 保存先を決定する。ファイルフォールバックはユーザーの同意を得た場合のみ使用する
 */
static bool selectBackendFromGui(const BackendSelector &selector, BackendSelection *selection)
{
	if (!selection) return false;
	for (;;) {
		*selection = selector.select(false);
		switch (selection->status) {
		case BackendSelection::Status::Ok:
			return true;
		case BackendSelection::Status::NeedsFileConsent: {
			const auto answer = QMessageBox::warning(
				nullptr, QObject::tr("Secure Storage Unavailable"),
				QObject::tr("The system secure storage (keychain / keyring) is not available.\n\n"
							"The Vault can be stored in a file instead. Anyone who obtains that file can attempt to guess "
							"your PIN offline, so its protection depends entirely on the strength of your PIN.\n\n"
							"If you previously created a Vault in the system keychain, choose Cancel, make the keychain "
							"available, and start the application again.\n\n"
							"Store the Vault in a file?"),
				QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
			if (answer != QMessageBox::Yes) return false;
			selection->status = BackendSelection::Status::Ok;
			return true;
		}
		case BackendSelection::Status::RecordedBackendUnavailable:
			if (!askRetry(QObject::tr("Vault Storage Unavailable"),
					QObject::tr("The Vault is stored in the %1 storage, which is not available now.\n\n"
								"If you use a system keyring, make sure it is running and unlocked, then retry.")
						.arg(QString::fromLatin1(BackendSelector::name(selection->kind))))) {
				return false;
			}
			continue;
		case BackendSelection::Status::ConfigError:
			QMessageBox::critical(nullptr, QObject::tr("Configuration Error"),
				QObject::tr("Failed to read the Vault storage configuration in:\n%1")
					.arg(pathToQString(appConfigDirectory())));
			return false;
		}
	}
}

/** セットアップ・リセット成功後、未記録の保存先を記録する */
static void recordBackend(const BackendSelector &selector, BackendSelection *selection)
{
	if (!selection || selection->recorded) return;
	selection->recorded = selector.record(selection->kind) == StorageStatus::Ok;
	if (!selection->recorded) {
		QMessageBox::warning(nullptr, QObject::tr("Configuration Error"),
			QObject::tr("Failed to record the Vault storage location. The Vault was created, but the application "
						"may not find it if the storage becomes temporarily unavailable."));
	}
}

static bool setupVaultFromGui(Vault *vault)
{
	if (!vault) return false;
	// SecureBuffer pin = PinDialog::setupPin(nullptr, &ok);
	SetupVaultDialog dlg(nullptr);
	if (dlg.exec() == QDialog::Accepted) {
		SecureBuffer pin;
		if (!dlg.pin(&pin)) {
			QMessageBox::critical(nullptr, QObject::tr("Setup Failed"), vaultErrorMessage(VaultError::MemoryLockFailed));
			return false;
		}
		if (pin.empty()) {
			QMessageBox::information(nullptr, QObject::tr("Cancelled"), QObject::tr("Vault setup was cancelled."));
			return false;
		}
		const VaultError err = vault->setup(pin);
		pin.clear();
		if (err != VaultError::None) {
			QMessageBox::critical(nullptr, QObject::tr("Setup Failed"), vaultErrorMessage(err));
			return false;
		}
		QMessageBox::information(nullptr, QObject::tr("Vault Setup"), QObject::tr("Vault has been set up successfully."));
		return true;
	}
	return false;
}

static bool resetVaultFromGui(Vault *vault)
{
	if (!vault) return false;
	const auto confirmation = QMessageBox::warning(
		nullptr, QObject::tr("Reset Vault"),
		QObject::tr("This permanently deletes the current Vault. Data encrypted with its current key cannot be recovered.\n\nContinue?"),
		QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);

	if (confirmation != QMessageBox::Yes) return false;

	// SecureBuffer newPin = PinDialog::setupPin(nullptr, &ok);
	ResetVaultDialog dlg(nullptr);
	if (dlg.exec() == QDialog::Accepted) {
		SecureBuffer newPin;
		if (!dlg.pin(&newPin)) {
			QMessageBox::critical(nullptr, QObject::tr("Reset Failed"), vaultErrorMessage(VaultError::MemoryLockFailed));
			return false;
		}
		if (newPin.empty()) return false;
		const VaultError err = vault->reset(newPin);
		newPin.clear();
		if (err != VaultError::None) {
			QMessageBox::critical(nullptr, QObject::tr("Reset Failed"), vaultErrorMessage(err));
			return false;
		}
		return true;
	}
	return false;
}

/** @return 解除できた場合 true。PIN 誤りは再入力を促し、それ以外のエラーで中断する */
static bool unlockVaultFromGui(Vault *vault)
{
	if (!vault) return false;
	for (;;) {
		// SecureBuffer pin = PinDialog::requestPin(nullptr, &ok);
		UnlockVaultDialog dlg(nullptr);
		if (dlg.exec() == QDialog::Accepted) {
			SecureBuffer pin;
			if (!dlg.pin(&pin)) {
				QMessageBox::critical(nullptr, QObject::tr("Unlock Failed"), vaultErrorMessage(VaultError::MemoryLockFailed));
				return false;
			}
			if (pin.empty()) return false;
			const VaultError err = vault->unlock(pin);
			pin.clear();
			if (err == VaultError::None) return true;
			if (err == VaultError::WrongPin) {
				QMessageBox::warning(nullptr, QObject::tr("Unlock Failed"), vaultErrorMessage(err));
				continue;
			}
			if (err == VaultError::BackendUnavailable) {
				if (askRetry(QObject::tr("Unlock Failed"), vaultErrorMessage(err))) continue;
				return false;
			}
			QMessageBox::critical(nullptr, QObject::tr("Unlock Failed"), vaultErrorMessage(err));
		} else {
			return false;
		}
	}
}

static int runGuiDemo(int argc, char *argv[])
{
	QApplication app(argc, argv);

	BackendSelector selector(appConfigDirectory(), "com.example.localvault.Vault", EMK_KEY);
	BackendSelection selection;
	if (!selectBackendFromGui(selector, &selection)) return 1;
	Vault vault(selection.backend.get(), EMK_KEY);

	for (bool ready = false; !ready;) {
		switch (vault.state()) {
		case VaultState::BackendUnavailable:
		case VaultState::StorageError:
			// EMK の有無が不明なまま新規セットアップへ進まない
			if (!askRetry(QObject::tr("Vault Storage Error"), vaultErrorMessage(vault.state() == VaultState::BackendUnavailable ? VaultError::BackendUnavailable : VaultError::StorageError))) {
				return 1;
			}
			break;
		case VaultState::NotSetup:
			if (!setupVaultFromGui(&vault)) return 0;
			recordBackend(selector, &selection);
			ready = true;
			break;
		case VaultState::Locked: {
			QMessageBox choice;
			choice.setWindowTitle(QObject::tr("Vault"));
			choice.setText(QObject::tr("Unlock the existing Vault, or permanently reset it?"));
			QPushButton *unlockButton = choice.addButton(QObject::tr("Unlock"), QMessageBox::AcceptRole);
			QPushButton *resetButton = choice.addButton(QObject::tr("Reset Vault"), QMessageBox::DestructiveRole);
			choice.addButton(QMessageBox::Cancel);
			choice.exec();

			if (choice.clickedButton() == unlockButton) {
				if (!unlockVaultFromGui(&vault)) return 0;
				ready = true;
			} else if (choice.clickedButton() == resetButton) {
				if (resetVaultFromGui(&vault)) {
					recordBackend(selector, &selection);
					ready = true;
				}
			} else {
				return 0;
			}
			break;
		}
		case VaultState::Unlocked:
			ready = true;
			break;
		}
	}

	Blob secret = QByteArrayToBlob("API_KEY=sk-abcdef1234567890");
	Blob encrypted;
	SecureBuffer decrypted;
	VaultError err = vault.encrypt(secret, &encrypted);
	if (err == VaultError::None) {
		err = vault.decryptToSecureBuffer(encrypted, &decrypted);
	}

	QString message;
	if (err == VaultError::None && LocalVaultTest::secureBufferEquals(decrypted, secret)) {
		message = QObject::tr("Vault unlocked and roundtrip verified successfully.\n\nStorage: %1\nDecrypted: %2")
					  .arg(stdToQString(selection.backend->name()))
					  .arg(stdToQString(std::string(reinterpret_cast<const char *>(decrypted.data()), decrypted.size())));
	} else {
		message = QObject::tr("Vault roundtrip failed: %1").arg(vaultErrorMessage(err));
	}
	decrypted.clear();

	QMessageBox::information(nullptr, QObject::tr("Vault Demo"), message);

	vault.lock();
	return 0;
}

// ===== CLI reset =====

static int runCliReset()
{
	BackendSelector selector(appConfigDirectory(), "com.example.localvault.Vault", EMK_KEY);
	BackendSelection selection = selector.select(false);
	switch (selection.status) {
	case BackendSelection::Status::Ok:
		break;
	case BackendSelection::Status::NeedsFileConsent:
		std::fprintf(stderr, "ERROR: The system secure storage is unavailable, so a Vault stored there cannot be checked. "
							 "No Vault was found in file storage.\n");
		return 1;
	case BackendSelection::Status::RecordedBackendUnavailable:
		std::fprintf(stderr, "ERROR: The Vault is stored in the %s storage, which is not available now. Nothing was removed.\n",
			BackendSelector::name(selection.kind));
		return 1;
	case BackendSelection::Status::ConfigError:
		std::fprintf(stderr, "ERROR: Failed to read the Vault storage configuration. Nothing was removed.\n");
		return 1;
	}

	Vault vault(selection.backend.get(), EMK_KEY);
	switch (vault.state()) {
	case VaultState::NotSetup:
		selector.forget();
		std::printf("No existing Vault was found.\n");
		return 0;
	case VaultState::BackendUnavailable:
	case VaultState::StorageError:
		std::fprintf(stderr, "ERROR: Failed to access the Vault storage. Nothing was removed.\n");
		return 1;
	case VaultState::Locked:
	case VaultState::Unlocked:
		break;
	}

	const VaultError err = vault.destroy();
	if (err != VaultError::None) {
		std::fprintf(stderr, "ERROR: Failed to remove the existing Vault: %s\n", toString(err));
		return 1;
	}
	if (selector.forget() == StorageStatus::Error) {
		std::fprintf(stderr, "WARNING: Failed to remove the storage location record.\n");
	}
	std::printf("Vault removed. Run the application normally to set up a new Vault.\n");
	return 0;
}

// ===== Entry point =====

int main(int argc, char *argv[])
{
	if (!hardenProcess()) {
		std::fprintf(stderr, "WARNING: Failed to disable core dumps.\n");
	}

	if (argc > 1 && QString::fromLatin1(argv[1]) == QStringLiteral("--test")) {
		return LocalVaultTest().runTests();
	}

	if (argc > 1 && QString::fromLatin1(argv[1]) == QStringLiteral("--reset")) {
		if (argc < 3 || QString::fromLatin1(argv[2]) != QStringLiteral("--yes")) {
			std::fprintf(stderr, "Refusing to reset without --yes. Usage: %s --reset --yes\n", argv[0]);
			return 2;
		}
		QCoreApplication app(argc, argv);
		return runCliReset();
	}

	return runGuiDemo(argc, argv);
}
