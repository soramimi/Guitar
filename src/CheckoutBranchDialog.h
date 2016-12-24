#ifndef CHECKOUTBRANCHDIALOG_H
#define CHECKOUTBRANCHDIALOG_H

#include <QDialog>
#include "Git.h"

namespace Ui {
class CheckoutBranchDialog;
}

class CheckoutBranchDialog : public QDialog
{
	Q_OBJECT

public:
	explicit CheckoutBranchDialog(QWidget *parent, QList<Git::Branch> const &branches);
	~CheckoutBranchDialog();

	QString branchName() const;
private:
	Ui::CheckoutBranchDialog *ui;
};

#endif // CHECKOUTBRANCHDIALOG_H
