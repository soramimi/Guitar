#ifndef RESETVAULTDIALOG_H
#define RESETVAULTDIALOG_H

#include <QDialog>

#include "../vault/SecureBuffer.h"

namespace Ui { class ResetVaultDialog; }

class ResetVaultDialog : public QDialog
{
	Q_OBJECT

public:
	explicit ResetVaultDialog(QWidget *parent = nullptr);
	~ResetVaultDialog();
	
	localvault::SecureBuffer pin() const;
	
private:
	Ui::ResetVaultDialog *ui;
	
	// QDialog interface
public slots:
	void accept();
};

#endif // RESETVAULTDIALOG_H
