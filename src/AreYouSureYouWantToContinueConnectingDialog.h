#ifndef AREYOUSUREYOUWANTTOCONTINUECONNECTINGDIALOG_H
#define AREYOUSUREYOUWANTTOCONTINUECONNECTINGDIALOG_H

#include <QDialog>

namespace Ui {
class AreYouSureYouWantToContinueConnectingDialog;
}

class AreYouSureYouWantToContinueConnectingDialog : public QDialog {
	Q_OBJECT
public:
	enum Result {
		Cancel,
		Yes,
		No,
	};
private:
	Ui::AreYouSureYouWantToContinueConnectingDialog *ui;
	Result result_ = Result::Cancel;
public:
	explicit AreYouSureYouWantToContinueConnectingDialog(QWidget *parent = nullptr);
	~AreYouSureYouWantToContinueConnectingDialog() override;

	void setLabel(const QString &label);
	Result result() const;

private slots:
	void on_pushButton_continue_clicked();
	void on_pushButton_close_clicked();

	// QDialog interface
public slots:
	void reject() override;
};

#endif // AREYOUSUREYOUWANTTOCONTINUECONNECTINGDIALOG_H
