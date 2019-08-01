#ifndef MERGEBRANCHDIALOG_H
#define MERGEBRANCHDIALOG_H

#include <QDialog>

namespace Ui {
class MergeBranchDialog;
}

class MergeBranchDialog : public QDialog {
	Q_OBJECT
public:
	explicit MergeBranchDialog(const QString &fastforward, std::vector<QString> const &labels, QString const curr_branch_name, QWidget *parent = nullptr);
	~MergeBranchDialog();

	QString getFastForwardPolicy() const;
	void setFastForwardPolicy(const QString &ff);
	QString mergeFrom() const;
private:
	Ui::MergeBranchDialog *ui;
};

#endif // MERGEBRANCHDIALOG_H
