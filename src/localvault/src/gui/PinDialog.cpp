#include "PinDialog.h"
#include "SecurePinEdit.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QMessageBox>

namespace localvault {

PinDialog::PinDialog(Mode mode, QWidget *parent)
	: QDialog(parent)
	, mode_(mode)
{
	setWindowTitle(mode == Mode::Setup ? tr("Set Vault PIN") : tr("Unlock Vault"));

	QFormLayout *formLayout = new QFormLayout(this);

	pinEdit_ = new SecurePinEdit(this);
	pinEdit_->setMaxLength(64);
	formLayout->addRow(mode == Mode::Setup ? tr("New PIN:") : tr("PIN:"), pinEdit_);

	if (mode == Mode::Setup) {
		confirmEdit_ = new SecurePinEdit(this);
		confirmEdit_->setMaxLength(64);
		formLayout->addRow(tr("Confirm PIN:"), confirmEdit_);
	}

	QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
	connect(buttons, &QDialogButtonBox::accepted, this, &PinDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &PinDialog::reject);
	formLayout->addRow(buttons);

	setLayout(formLayout);
	pinEdit_->setFocus();
}

PinDialog::~PinDialog() = default;

SecureBuffer PinDialog::pin() const
{
	return pinEdit_->pin();
}

void PinDialog::clearInput()
{
	pinEdit_->clear();
	if (confirmEdit_) {
		confirmEdit_->clear();
	}
}

void PinDialog::accept()
{
	if (!allow_empty_pin && pinEdit_->isEmpty()) {
		QMessageBox::warning(this, tr("Invalid PIN"), tr("PIN cannot be empty."));
		return;
	}
	if (mode_ == Mode::Setup && confirmEdit_ && !pinEdit_->equals(*confirmEdit_)) {
		QMessageBox::warning(this, tr("PIN Mismatch"), tr("The two PINs do not match."));
		clearInput();
		pinEdit_->setFocus();
		return;
	}

	QDialog::accept();
}

void PinDialog::reject()
{
	clearInput();
	QDialog::reject();
}

SecureBuffer PinDialog::requestPin(QWidget *parent, bool *ok)
{
	PinDialog dialog(Mode::Unlock, parent);
	bool accepted = (dialog.exec() == QDialog::Accepted);
	if (ok) {
		*ok = accepted;
	}
	if (accepted) {
		return dialog.pin();
	}
	return SecureBuffer();
}

SecureBuffer PinDialog::setupPin(QWidget *parent, bool *ok)
{
	PinDialog dialog(Mode::Setup, parent);
	bool accepted = (dialog.exec() == QDialog::Accepted);
	if (ok) {
		*ok = accepted;
	}
	if (accepted) {
		return dialog.pin();
	}
	return SecureBuffer();
}

} // namespace localvault
