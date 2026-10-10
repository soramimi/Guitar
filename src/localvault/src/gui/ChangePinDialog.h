#ifndef CHANGEPINDIALOG_H
#define CHANGEPINDIALOG_H

#include "SecureStoreGUI.h"
#include <QDialog>

namespace localvault {
class VaultWithBackend;
}

namespace Ui { class ChangePinDialog; }

class ChangePinDialog : public QDialog {
	Q_OBJECT
private:
	Ui::ChangePinDialog *ui;
	QString schema_;
	localvault::VaultWithBackend *global_vault_ = nullptr;
	localvault::StoragePreference storagePreference_ = localvault::StoragePreference::PreferSystem;
public:
	explicit ChangePinDialog(QWidget *parent, QString const &schema, localvault::VaultWithBackend *global_vault,
		localvault::StoragePreference storagePreference = localvault::StoragePreference::PreferSystem);
	~ChangePinDialog();

public slots:
	void accept();
};

#endif // CHANGEPINDIALOG_H
