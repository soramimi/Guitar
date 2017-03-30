#ifndef CHECKOUTDIALOG_H
#define CHECKOUTDIALOG_H

#include <QDialog>
#include "Git.h"

namespace Ui {
class CheckoutDialog;
}

class CheckoutDialog : public QDialog
{
	Q_OBJECT
private:
	struct Private;
	Private *pv;
public:

	explicit CheckoutDialog(QWidget *parent, const QStringList &tags, const QStringList &local_branches, const QStringList &remote_branches);
	~CheckoutDialog();

	enum class Operation {
		HeadDetached,
		CreateLocalBranch,
		ExistingLocalBranch,
	};

	Operation operation() const;

	QString branchName() const;
private slots:
	void on_radioButton_head_detached_toggled(bool checked);

	void on_radioButton_existing_local_branch_toggled(bool checked);

	void on_radioButton_create_local_branch_toggled(bool checked);

private:
	Ui::CheckoutDialog *ui;
	void updateUI();
	int makeComboBoxOptions(const QStringList &names);
	void clearComboBoxOptions();
};

#endif // CHECKOUTDIALOG_H
