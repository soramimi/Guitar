#include "SetupVaultDialog.h"
#include "ui_SetupVaultDialog.h"

#include <QMessageBox>

SetupVaultDialog::SetupVaultDialog(
	QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::SetupPinDialog)
{
	ui->setupUi(this);
}

SetupVaultDialog::~SetupVaultDialog()
{
	delete ui;
}

localvault::SecureBuffer SetupVaultDialog::pin() const
{
	return ui->widget_new_pin->pin();
}

void SetupVaultDialog::accept()
{
	localvault::SecureBuffer newpin = ui->widget_new_pin->pin();
	localvault::SecureBuffer confirmpin = ui->widget_confirm_pin->pin();
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
