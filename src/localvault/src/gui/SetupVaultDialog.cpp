#include "SetupVaultDialog.h"
#include "ui_SetupVaultDialog.h"

#include <QMessageBox>

SetupVaultDialog::SetupVaultDialog(
	QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::SetupPinDialog)
{
	ui->setupUi(this);
	
	ui->label_no_pin->setVisible(localvault::allow_empty_pin);
}

SetupVaultDialog::~SetupVaultDialog()
{
	delete ui;
}

bool SetupVaultDialog::pin(localvault::SecureBuffer *out) const
{
	return ui->widget_new_pin->pin(out);
}

void SetupVaultDialog::accept()
{
	localvault::SecureBuffer newpin;
	localvault::SecureBuffer confirmpin;
	if (!ui->widget_new_pin->pin(&newpin) || !ui->widget_confirm_pin->pin(&confirmpin)) {
		QMessageBox::critical(this, tr("PIN Error"), tr("Failed to lock memory for the PIN."));
		return;
	}
	if (!localvault::validate_pin(newpin)) {
		QMessageBox::warning(this, tr("Invalid PIN"), tr("The new PIN is invalid."));
		return;
	}
	if (!newpin.equals(confirmpin)) {
		QMessageBox::warning(this, tr("PIN Mismatch"), tr("The two PINs do not match."));
		return;
	}
	done(QDialog::Accepted);
}
