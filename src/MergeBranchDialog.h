#ifndef MERGEBRANCHDIALOG_H
#define MERGEBRANCHDIALOG_H

#include <QDialog>
#include "Git.h"

namespace Ui {
class MergeBranchDialog;
}

class MergeBranchDialog : public QDialog {
	Q_OBJECT
public:
	explicit MergeBranchDialog(QWidget *parent, QList<Git::Branch> const &branches);
	~MergeBranchDialog() override;

	QString branchName() const;
private:
	Ui::MergeBranchDialog *ui;
};

#endif // MERGEBRANCHDIALOG_H
