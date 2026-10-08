#include "UnlockVaultDialog.h"
#include "ui_UnlockVaultDialog.h"

UnlockVaultDialog::UnlockVaultDialog(QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::VaultDialog)
{
	ui->setupUi(this);
	
	// ui->checkBox_confirm_destroy->setChecked(false);
	// ui->pushButton_destroy->setEnabled(false);
	// ui->frame_destroy->setVisible(false);
}

UnlockVaultDialog::~UnlockVaultDialog()
{
	delete ui;
}

UnlockVaultDialog::Result UnlockVaultDialog::result() const
{
	return result_;
}

localvault::SecureBuffer UnlockVaultDialog::pin() const
{
	return ui->widget->pin();
}

// void UnlockVaultDialog::on_checkBox_confirm_destroy_checkStateChanged(const Qt::CheckState &arg1)
// {
// 	ui->pushButton_destroy->setEnabled(ui->checkBox_confirm_destroy->isChecked());
// }

// void UnlockVaultDialog::on_pushButton_destroy_clicked()
// {
// 	if (ui->checkBox_confirm_destroy->isChecked()) {
// 		result_ = UnlockVaultDialog::Destroy;
// 		done(QDialog::Accepted);
// 	}
// }

// void UnlockVaultDialog::on_pushButton_reset_pin_clicked()
// {
// 	result_ = UnlockVaultDialog::Reset;
// 	done(QDialog::Accepted);
// }


void UnlockVaultDialog::on_pushButton_unlock_clicked()
{
	result_ = UnlockVaultDialog::Unlock;
	done(QDialog::Accepted);
}

