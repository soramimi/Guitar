#ifndef SECURESTOREGUI_H
#define SECURESTOREGUI_H

#include <QPushButton>
#include <app/BackendSelector.h>
#include <storage/FileBackend.h>
#include <storage/SystemKeychainBackend.h>
#include <vault/Vault.h>
#include <memory>

namespace localvault {

struct VaultWithBackend {
	BackendSelection backend;
	std::unique_ptr<Vault> vault;
	operator bool () const
	{
		return (bool)vault;
	}
	void reset()
	{
		vault.reset();
		backend = {};
	}
};

class SecureStoreGUI {
private:
	QWidget *parent_ = nullptr;
	bool force_file_backend_ = false;
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
	SecureStoreGUI(QWidget *parent, bool force_file_backend);
	VaultWithBackend execUnlock(QString const &confdir, QString const &schema);
	VaultWithBackend execUnlock2(QString const &confdir, QString const &schema, localvault::SecureBuffer const &pin);
	void setForceFileBackend(bool force_file_backend);
};

} // namespace localvault


#endif // SECURESTOREGUI_H
