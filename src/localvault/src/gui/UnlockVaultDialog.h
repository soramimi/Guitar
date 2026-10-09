#ifndef UnlockVaultDialog_H
#define UnlockVaultDialog_H

#include <QDialog>

#include "../vault/SecureBuffer.h"

namespace Ui { class VaultDialog; }

class UnlockVaultDialog : public QDialog
{
	Q_OBJECT
public:
	enum Result {
		Cancel,
		Unlock,
	};
private:
	Result result_ = Cancel;
public:
	explicit UnlockVaultDialog(QWidget *parent);
	~UnlockVaultDialog();

	localvault::SecureBuffer pin() const;

	Result result() const;

private slots:
	void on_pushButton_unlock_clicked();

private:
	Ui::VaultDialog *ui;
};

#endif // UnlockVaultDialog_H
