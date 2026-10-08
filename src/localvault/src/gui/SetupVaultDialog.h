#ifndef SETUPVAULTDIALOG_H
#define SETUPVAULTDIALOG_H

#include <QDialog>

#include <vault/SecureBuffer.h>

namespace Ui { class SetupPinDialog; }

class SetupVaultDialog : public QDialog
{
	Q_OBJECT

public:
	explicit SetupVaultDialog(QWidget *parent = nullptr);
	~SetupVaultDialog();
	
	localvault::SecureBuffer pin() const;
private:
	Ui::SetupPinDialog *ui;
	
	// QDialog interface
public slots:
	void accept();
};

#endif // SETUPVAULTDIALOG_H
