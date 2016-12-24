#ifndef NEWBRANCHDIALOG_H
#define NEWBRANCHDIALOG_H

#include <QDialog>

namespace Ui {
class NewBranchDialog;
}

class NewBranchDialog : public QDialog
{
	Q_OBJECT

public:
	explicit NewBranchDialog(QWidget *parent = 0);
	~NewBranchDialog();

	QString branchName() const;
private:
	Ui::NewBranchDialog *ui;
};

#endif // NEWBRANCHDIALOG_H
