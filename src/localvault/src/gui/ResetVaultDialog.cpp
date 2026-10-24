#include "ResetVaultDialog.h"
#include "ui_ResetVaultDialog.h"

#include <QMessageBox>

ResetVaultDialog::ResetVaultDialog(
	QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::ResetVaultDialog)
{
	ui->setupUi(this);
	
	ui->label_no_pin->setVisible(localvault::isEmptyPinAllowed());
}

ResetVaultDialog::~ResetVaultDialog()
{
	delete ui;
}

bool ResetVaultDialog::pin(localvault::SecureBuffer *out) const
{
	return ui->widget_new_pin->pin(out);
}

void ResetVaultDialog::accept()
{
	if (!localvault::isEmptyPinAllowed() && ui->widget_new_pin->isEmpty()) {
		QMessageBox::warning(this, tr("Invalid PIN"), tr("PIN cannot be empty."));
		return;
	}
	if (ui->widget_new_pin->equals(*ui->widget_confirm_pin)) {
		QDialog::accept();
	} else {
		QMessageBox::warning(this, tr("PIN Mismatch"), tr("The two PINs do not match."));
		ui->widget_new_pin->clear();
		ui->widget_confirm_pin->clear();
		ui->widget_new_pin->setFocus();
	}
}
