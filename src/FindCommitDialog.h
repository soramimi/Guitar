#ifndef FINDCOMMITDIALOG_H
#define FINDCOMMITDIALOG_H

#include <QDialog>

namespace Ui {
class FindCommitDialog;
}

class FindCommitDialog : public QDialog {
	Q_OBJECT

public:
	explicit FindCommitDialog(QWidget *parent, const QString &text);
	~FindCommitDialog() override;

	QString text() const;
private:
	Ui::FindCommitDialog *ui;
};

#endif // FINDCOMMITDIALOG_H
