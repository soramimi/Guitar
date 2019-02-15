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
	Private *m;
public:

	explicit CheckoutDialog(QWidget *parent, QStringList const &tags, const QStringList &all_local_branches, QStringList const &local_branches, QStringList const &remote_branches);
	~CheckoutDialog() override;

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
	int makeComboBoxOptionsFromList(QStringList const &names);
	Operation makeComboBoxOptions(bool click);
	void clearComboBoxOptions();
};

#endif // CHECKOUTDIALOG_H
