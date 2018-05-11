#ifndef AREYOUSUREYOUWANTTOCONTINUECONNECTINGDIALOG_H
#define AREYOUSUREYOUWANTTOCONTINUECONNECTINGDIALOG_H

#include <QDialog>

namespace Ui {
class AreYouSureYouWantToContinueConnectingDialog;
}

class AreYouSureYouWantToContinueConnectingDialog : public QDialog
{
	Q_OBJECT
public:
	enum Result {
		Cancel,
		Yes,
		No,
	};
public:
	explicit AreYouSureYouWantToContinueConnectingDialog(QWidget *parent = 0);
	~AreYouSureYouWantToContinueConnectingDialog();

	Result result() const;

private slots:
	void on_pushButton_continue_clicked();
	void on_pushButton_close_clicked();
private:
	Ui::AreYouSureYouWantToContinueConnectingDialog *ui;
	Result result_ = Result::Cancel;

	// QDialog interface
public slots:
	void reject();
};

#endif // AREYOUSUREYOUWANTTOCONTINUECONNECTINGDIALOG_H
