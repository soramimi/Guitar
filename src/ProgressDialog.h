#ifndef PROGRESSDIALOG_H
#define PROGRESSDIALOG_H

#include <QDialog>

namespace Ui {
class ProgressDialog;
}

class ProgressDialog : public QDialog
{
	Q_OBJECT
private:
	bool canceled_by_user = false;
public:
	explicit ProgressDialog(QWidget *parent = 0);
	~ProgressDialog();

	void setLabelText(const QString &text);
	bool isCanceledByUser() const;
private slots:
	void on_pushButton_cancel_clicked();

private:
	Ui::ProgressDialog *ui;
};

#endif // PROGRESSDIALOG_H
