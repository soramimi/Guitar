#include "ChangePinDialog.h"
#include "ui_ChangePinDialog.h"
#include <vault/SecureBuffer.h>
#include <vault/Vault.h>
#include <storage/FileBackend.h>
#include "SecureStoreGUI.h"
#include <QMessageBox>

#ifdef APP_GUITAR
#include "ApplicationGlobal.h"
static QString getConfigDirectory()
{
	return global->app_secret_config_dir;
	
}
#else
static QString getConfigDirectory()
{
	auto path = localvault::FileBackend::defaultConfigDirectory();
	return localvault::SecureStoreGUI::pathToQString(path);
}
#endif

ChangePinDialog::ChangePinDialog(QWidget *parent, QString const &schema, localvault::VaultWithBackend *global_vault,
	localvault::StoragePreference storagePreference)
	: QDialog(parent)
	, ui(new Ui::ChangePinDialog)
	, schema_(schema)
	, global_vault_(global_vault)
	, storagePreference_(storagePreference)
{
	ui->setupUi(this);
	
	ui->label_no_pin->setVisible(localvault::allow_empty_pin);
}

ChangePinDialog::~ChangePinDialog()
{
	delete ui;
}

void ChangePinDialog::accept()
{
	localvault::SecureBuffer oldPin;
	localvault::SecureBuffer newPin;
	localvault::SecureBuffer confirmPin;
	if (!ui->widget_old_pin->pin(&oldPin)
		|| !ui->widget_new_pin->pin(&newPin)
		|| !ui->widget_confirm_pin->pin(&confirmPin)) {
		QMessageBox::critical(this, tr("PIN Error"), localvault::SecureStoreGUI::vaultErrorMessage(localvault::VaultError::MemoryLockFailed));
		return;
	}
	if (!localvault::validate_pin(newPin)) {
		QMessageBox::warning(this, tr("Invalid PIN"), tr("The new PIN is invalid."));
		return;
	}
	if (!newPin.equals(confirmPin)) {
		QMessageBox::warning(this, tr("PIN Mismatch"), tr("The two PINs do not match."));
		return;
	}
	
	QString confdir = getConfigDirectory();
	// QString schema_ = global->vault_schema();
	
	localvault::SecureStoreGUI store(this, storagePreference_);
	
	auto vault = store.execUnlock2(confdir, schema_, oldPin);
	if (!vault) {
		QMessageBox::critical(this, tr("Error"), tr("Failed to unlock the vault with the old PIN."));
		return;
	}
	
	auto r = vault.vault->changePin(oldPin, newPin);
	if (r == localvault::VaultError::None) {
		if (global_vault_) {
			*global_vault_ = std::move(vault);
		}
		QMessageBox::information(this, tr("PIN Changed"), tr("The PIN has been changed successfully."));
		done(QDialog::Accepted);
		return;
	}

	QMessageBox::critical(this, tr("Error"), localvault::SecureStoreGUI::vaultErrorMessage(r));
}

