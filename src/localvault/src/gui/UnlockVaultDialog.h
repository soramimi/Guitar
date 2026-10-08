#ifndef UnlockVaultDialog_H
#define UnlockVaultDialog_H

#include <QDialog>

#include "../vault/Vault.h"

namespace Ui { class VaultDialog; }

class UnlockVaultDialog : public QDialog
{
	Q_OBJECT
public:
	enum Result {
		Cancel,
		Unlock,
		Reset,
		Destroy,
	};
private:
	Result result_ = Cancel;
public:
	explicit UnlockVaultDialog(QWidget *parent);
	~UnlockVaultDialog();
	
	localvault::SecureBuffer pin() const;
	
	Result result() const;
	
private slots:
	
	// void on_checkBox_confirm_destroy_checkStateChanged(const Qt::CheckState &arg1);
	
	// void on_pushButton_destroy_clicked();
	
	// void on_pushButton_reset_pin_clicked();
	
	void on_pushButton_unlock_clicked();
	
private:
	Ui::VaultDialog *ui;
};

#endif // UnlockVaultDialog_H
