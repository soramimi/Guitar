#include "UnlockVaultDialog.h"
#include "ui_UnlockVaultDialog.h"

UnlockVaultDialog::UnlockVaultDialog(QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::VaultDialog)
{
	ui->setupUi(this);
	
	ui->label_no_pin->setVisible(localvault::allow_empty_pin);
}

UnlockVaultDialog::~UnlockVaultDialog()
{
	delete ui;
}

UnlockVaultDialog::Result UnlockVaultDialog::result() const
{
	return result_;
}

bool UnlockVaultDialog::pin(localvault::SecureBuffer *out) const
{
	return ui->widget->pin(out);
}

void UnlockVaultDialog::on_pushButton_unlock_clicked()
{
	result_ = UnlockVaultDialog::Unlock;
	done(QDialog::Accepted);
}

