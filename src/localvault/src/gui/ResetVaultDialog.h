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
	
	bool pin(localvault::SecureBuffer *out) const;
	
private:
	Ui::ResetVaultDialog *ui;
	
	// QDialog interface
public slots:
	void accept();
};

#endif // RESETVAULTDIALOG_H
