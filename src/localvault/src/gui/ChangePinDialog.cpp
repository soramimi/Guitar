#include "ChangePinDialog.h"
#include "ui_ChangePinDialog.h"
#include <vault/SecureBuffer.h>
#include "ApplicationGlobal.h"
#include "SecureStoreGUI.h"
#include <QMessageBox>

ChangePinDialog::ChangePinDialog(
	QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::ChangePinDialog)
{
	ui->setupUi(this);
}

ChangePinDialog::~ChangePinDialog()
{
	delete ui;
}

void ChangePinDialog::accept()
{
	localvault::SecureBuffer oldPin = ui->widget_old_pin->pin();
	localvault::SecureBuffer newPin = ui->widget_new_pin->pin();
	localvault::SecureBuffer confirmPin = ui->widget_confirm_pin->pin();
	if (!localvault::validate_pin(newPin)) {
		QMessageBox::warning(this, tr("Invalid PIN"), tr("The new PIN is invalid."));
		return;
	}
	if (!newPin.equals(confirmPin)) {
		QMessageBox::warning(this, tr("PIN Mismatch"), tr("The two PINs do not match."));
		return;
	}
	
	QString confdir = global->app_secret_config_dir;
	QString schema = "jp.soramimi.GenerativeAI";
	
	constexpr bool force_file_backend = true;
	localvault::SecureStoreGUI store(this, force_file_backend);
	
	auto vault = store.execUnlock2(confdir, schema, std::move(oldPin));
	if (!vault) {
		QMessageBox::critical(this, tr("Error"), tr("Failed to unlock the vault with the old PIN."));
		return;
	}
	
	auto r = vault.vault->changePin(oldPin, newPin);
	if (r == localvault::VaultError::None) {
		if (global->vault) {
			global->vault.reset();
		}
		global->vault = std::move(vault);
		QMessageBox::information(this, tr("PIN Changed"), tr("The PIN has been changed successfully."));
		done(QDialog::Accepted);
		return;
	}
	
	QMessageBox::critical(this, tr("Error"), localvault::SecureStoreGUI::vaultErrorMessage(r));
}


