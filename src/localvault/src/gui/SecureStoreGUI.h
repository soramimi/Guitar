#ifndef SECURESTOREGUI_H
#define SECURESTOREGUI_H

#include "../app/BackendSelector.h"
#include "../app/SecretVault.h"
#include "../storage/FileBackend.h"
#include "../storage/SystemKeychainBackend.h"
#include "../vault/Vault.h"
#include <QString>
#include <memory>

class QWidget;

namespace localvault {

enum class StoragePreference {
	/** OS のセキュアストレージを優先し、使えない初回だけ同意を求めて FileBackend を使う */
	PreferSystem,
	/** OS のセキュアストレージを探索せず、常に FileBackend を使う */
	FileOnly
};

class SecureStoreGUI {
private:
	QWidget *parent_ = nullptr;
	StoragePreference storagePreference_ = StoragePreference::PreferSystem;
public:
	static constexpr char EMK_KEY[] = "vault_emk";
	
	static QString vaultErrorMessage(VaultError error);
public:
	std::unique_ptr<ISecretStorageBackend> createSystemBackend();
public:
	static Blob QByteArrayToBlob(const QByteArray &ba);
	static std::string QStringToStd(QString const &s);
	static QString stdToQString(std::string const &s);
	static std::filesystem::path QStringToPath(QString const &s);
	static QString pathToQString(const std::filesystem::path &p);
	static bool secureBufferEquals(SecureBuffer const &buf, Blob const &blob);
private:
	
	bool resetVault(Vault *vault);
	int destroyVault(const QString &confdir, const QString &schema);
	bool setupVault(Vault *vault, bool msgbox);
	bool askRetry(QString const &title, QString const &message);
	std::filesystem::path appConfigDirectory();
	bool selectBackend(const BackendSelector &selector, BackendSelection *selection, bool force_file_backend);
	void recordBackend(const BackendSelector &selector, BackendSelection *selection);
	bool unlockVault(Vault *vault, const SecureBuffer &pin);
	localvault::BackendSelector makeBackendSelector(const QString &confdir, const QString &schema);
public:
	SecureStoreGUI(QWidget *parent, StoragePreference storagePreference = StoragePreference::PreferSystem);
	VaultWithBackend execUnlock(QString const &confdir, QString const &schema);
	VaultWithBackend execUnlock2(QString const &confdir, QString const &schema, localvault::SecureBuffer const &pin);
	void setStoragePreference(StoragePreference storagePreference);
	StoragePreference storagePreference() const;
};

} // namespace localvault


#endif // SECURESTOREGUI_H
