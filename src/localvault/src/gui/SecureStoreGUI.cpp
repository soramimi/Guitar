#include "SecureStoreGUI.h"
#include "ResetVaultDialog.h"
#include "UnlockVaultDialog.h"
#include "SetupVaultDialog.h"
#include <QMessageBox>
#include <QStandardPaths>

QString localvault::SecureStoreGUI::vaultErrorMessage(VaultError error)
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

std::unique_ptr<localvault::ISecretStorageBackend> localvault::SecureStoreGUI::createSystemBackend()
{
	auto systemBackend = std::make_unique<SystemKeychainBackend>();
	if (systemBackend->isAvailable()) {
		return systemBackend;
	}
	return {};
}

localvault::Blob localvault::SecureStoreGUI::QByteArrayToBlob(const QByteArray &ba)
{
	return Blob(ba.constData(), ba.constData() + ba.size());
}

std::string localvault::SecureStoreGUI::QStringToStd(const QString &s)
{
	return s.toUtf8().toStdString();
}

QString localvault::SecureStoreGUI::stdToQString(const std::string &s)
{
	return QString::fromUtf8(s.c_str(), static_cast<int>(s.size()));
}

std::filesystem::path localvault::SecureStoreGUI::QStringToPath(const QString &s)
{
#if defined(_WIN32)
	// Windows では narrow 文字列が ANSI コードページとして解釈されるため、UTF-16 で渡す
	return std::filesystem::path(s.toStdWString());
#else
	return std::filesystem::path(QStringToStd(s));
#endif
}

QString localvault::SecureStoreGUI::pathToQString(const std::filesystem::path &p)
{
#if defined(_WIN32)
	return QString::fromStdWString(p.wstring());
#else
	return stdToQString(p.string());
#endif
}

bool localvault::SecureStoreGUI::secureBufferEquals(const SecureBuffer &buf, const Blob &blob)
{
	return buf.size() == blob.size() && std::equal(buf.begin(), buf.end(), blob.begin(), blob.end());
}

bool localvault::SecureStoreGUI::resetVault(Vault *vault)
{
	if (!vault) return false;
	const auto confirmation = QMessageBox::warning(
				parent_, QObject::tr("Reset Vault"),
				QObject::tr("This permanently deletes the current Vault. Data encrypted with its current key cannot be recovered.\n\nContinue?"),
				QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
	
	if (confirmation != QMessageBox::Yes) return false;
	
	ResetVaultDialog dlg(parent_);
	if (dlg.exec() == QDialog::Rejected) return false;
	
	SecureBuffer newPin;
	if (!dlg.pin(&newPin)) {
		QMessageBox::critical(parent_, QObject::tr("Reset Failed"), vaultErrorMessage(VaultError::MemoryLockFailed));
		return false;
	}
	const VaultError err = vault->reset(newPin);
	newPin.clear();
	if (err != VaultError::None) {
		QMessageBox::critical(parent_, QObject::tr("Reset Failed"), vaultErrorMessage(err));
		return false;
	}
	return true;
}

localvault::BackendSelector localvault::SecureStoreGUI::makeBackendSelector(QString const &confdir, QString const &schema)
{
	auto dir = QStringToPath(confdir);
	return BackendSelector(dir, schema.toStdString(), EMK_KEY);
}

int localvault::SecureStoreGUI::destroyVault(QString const &confdir, QString const &schema)
{
	BackendSelection selection;
	BackendSelector selector = makeBackendSelector(confdir, schema);
	if (!selectBackend(selector, &selection, storagePreference_ == StoragePreference::FileOnly)) return {};
	
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

bool localvault::SecureStoreGUI::setupVault(Vault *vault, bool msgbox)
{
	if (!vault) return false;
	SetupVaultDialog dlg(parent_);
	if (dlg.exec() == QDialog::Accepted) {
		SecureBuffer pin;
		if (!dlg.pin(&pin)) {
			if (msgbox) QMessageBox::critical(parent_, QObject::tr("Setup Failed"), vaultErrorMessage(VaultError::MemoryLockFailed));
			return false;
		}
		if (!validate_pin(pin)) {
			if (msgbox) QMessageBox::warning(parent_, QObject::tr("Invalid PIN"), QObject::tr("The PIN is invalid."));
			return false;
		}
		const VaultError err = vault->setup(pin);
		pin.clear();
		if (err != VaultError::None) {
			if (msgbox) QMessageBox::critical(parent_, QObject::tr("Setup Failed"), vaultErrorMessage(err));
			return false;
		}
		if (msgbox) QMessageBox::information(parent_, QObject::tr("Vault Setup"), QObject::tr("Vault has been set up successfully."));
		return true;
	}
	return false;
}

bool localvault::SecureStoreGUI::askRetry(const QString &title, const QString &message)
{
	return QMessageBox::critical(parent_, title, message, QMessageBox::Retry | QMessageBox::Cancel, QMessageBox::Retry) == QMessageBox::Retry;
}

std::filesystem::path localvault::SecureStoreGUI::appConfigDirectory()
{
	return QStringToPath(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
}

/**
 * @brief 保存先を決定する。ファイルフォールバックはユーザーの同意を得た場合のみ使用する
 */
bool localvault::SecureStoreGUI::selectBackend(const BackendSelector &selector, BackendSelection *selection, bool force_file_backend)
{
	if (!selection) return false;
	for (;;) {
		*selection = selector.select(force_file_backend);
		switch (selection->status) {
		case BackendSelection::Status::Ok:
			return true;
		case BackendSelection::Status::NeedsFileConsent: {
				const auto answer = QMessageBox::warning(
							parent_, QObject::tr("Secure Storage Unavailable"),
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
			QMessageBox::critical(parent_, QObject::tr("Configuration Error"),
								  QObject::tr("Failed to read the Vault storage configuration in:\n%1")
								  .arg(pathToQString(appConfigDirectory())));
			return false;
		}
	}
}

/** セットアップ・リセット成功後、未記録の保存先を記録する */
void localvault::SecureStoreGUI::recordBackend(const BackendSelector &selector, BackendSelection *selection)
{
	if (!selection || selection->recorded) return;
	selection->recorded = selector.record(selection->kind) == StorageStatus::Ok;
	if (!selection->recorded) {
		QMessageBox::warning(parent_, QObject::tr("Configuration Error"),
							 QObject::tr("Failed to record the Vault storage location. The Vault was created, but the application "
										 "may not find it if the storage becomes temporarily unavailable."));
	}
}

/** @return 解除できた場合 true。PIN 誤りは再入力を促し、それ以外のエラーで中断する */
bool localvault::SecureStoreGUI::unlockVault(Vault *vault, SecureBuffer const &pin)
{
	if (!vault) return false;

	if (!validate_pin(pin)) return false;

	const VaultError err = vault->unlock(pin);

	if (err == VaultError::None) return true;

	if (err == VaultError::WrongPin) {
		QMessageBox::warning(parent_, QObject::tr("Unlock Failed"), vaultErrorMessage(err));
	} else {
		QMessageBox::critical(parent_, QObject::tr("Unlock Failed"), vaultErrorMessage(err));
	}
	return false;
}

localvault::SecureStoreGUI::SecureStoreGUI(QWidget *parent, StoragePreference storagePreference)
	: parent_(parent)
	, storagePreference_(storagePreference)
{
}

localvault::VaultWithBackend localvault::SecureStoreGUI::execUnlock(QString const &confdir, QString const &schema)
{
	BackendSelection selection;
	BackendSelector selector = makeBackendSelector(confdir, schema);
	if (!selectBackend(selector, &selection, storagePreference_ == StoragePreference::FileOnly)) return {};
	
	std::unique_ptr<localvault::Vault> vault = std::make_unique<localvault::Vault>(selection.backend.get(), EMK_KEY);
	
	for (bool ready = false; !ready;) {
		switch (vault->state()) {
		case VaultState::BackendUnavailable:
			QMessageBox::critical(parent_, QObject::tr("Vault Storage Error"), vaultErrorMessage(VaultError::BackendUnavailable));
			return {};
		case VaultState::StorageError:
			QMessageBox::critical(parent_, QObject::tr("Vault Storage Error"), vaultErrorMessage(VaultError::StorageError));
			return {};
		case VaultState::NotSetup:
			if (!setupVault(vault.get(), true)) return {};
			recordBackend(selector, &selection);
			ready = true;
			break;
		case VaultState::Locked: {
				UnlockVaultDialog dlg(parent_);
				if (dlg.exec() == QDialog::Rejected) return {};

				if (dlg.result() == UnlockVaultDialog::Unlock) {
					SecureBuffer pin;
					if (!dlg.pin(&pin)) {
						QMessageBox::critical(parent_, QObject::tr("Unlock Failed"), vaultErrorMessage(VaultError::MemoryLockFailed));
						return {};
					}
					if (unlockVault(vault.get(), pin)) {
						ready = true;
					}
				} else {
					return {};
				}
			}
			break;
		case VaultState::Unlocked:
			ready = true;
			break;
		}
	}
	
	return {std::move(selection), std::move(vault)};
}

localvault::VaultWithBackend localvault::SecureStoreGUI::execUnlock2(QString const &confdir, QString const &schema, localvault::SecureBuffer const &pin)
{
	BackendSelection selection;
	BackendSelector selector = makeBackendSelector(confdir, schema);
	if (!selectBackend(selector, &selection, storagePreference_ == StoragePreference::FileOnly)) return {};

	std::unique_ptr<localvault::Vault> vault = std::make_unique<localvault::Vault>(selection.backend.get(), EMK_KEY);

	for (bool ready = false; !ready;) {
		switch (vault->state()) {
		case VaultState::BackendUnavailable:
			QMessageBox::critical(parent_, QObject::tr("Vault Storage Error"), vaultErrorMessage(VaultError::BackendUnavailable));
			return {};
		case VaultState::StorageError:
			QMessageBox::critical(parent_, QObject::tr("Vault Storage Error"), vaultErrorMessage(VaultError::StorageError));
			return {};
		case VaultState::NotSetup:
			QMessageBox::critical(parent_, QObject::tr("Vault Error"), vaultErrorMessage(VaultError::NotSetup));
			return {};
		case VaultState::Locked: {
				if (!unlockVault(vault.get(), pin)) {
					return {};
				}
				ready = true;
			}
			break;
		case VaultState::Unlocked:
			ready = true;
			break;
		}
	}

	return {std::move(selection), std::move(vault)};
}

void localvault::SecureStoreGUI::setStoragePreference(StoragePreference storagePreference)
{
	storagePreference_ = storagePreference;
}

localvault::StoragePreference localvault::SecureStoreGUI::storagePreference() const
{
	return storagePreference_;
}

