#ifndef MERGEDIALOG_H
#define MERGEDIALOG_H

#include <QDialog>

namespace Ui {
class MergeDialog;
}

class MergeDialog : public QDialog {
	Q_OBJECT
public:
	explicit MergeDialog(const QString &fastforward, std::vector<QString> const &labels, QString const curr_branch_name, QWidget *parent = nullptr);
	~MergeDialog();

	QString getFastForwardPolicy() const;
	void setFastForwardPolicy(const QString &ff);
	QString mergeFrom() const;
private:
	Ui::MergeDialog *ui;
};

#endif // MERGEDIALOG_H
