#ifndef PUSHDIALOG_H
#define PUSHDIALOG_H

#include <QDialog>

namespace Ui {
class PushDialog;
}

class PushDialog : public QDialog
{
	Q_OBJECT
public:
	explicit PushDialog(QWidget *parent = 0);
	~PushDialog();

	void setURL(const QString &url);
private slots:
	void on_lineEdit_url_textChanged(const QString &arg1);

	void on_lineEdit_username_textChanged(const QString &arg1);

	void on_lineEdit_password_cursorPositionChanged(int arg1, int arg2);

private:
	Ui::PushDialog *ui;
	void makeCompleteURL();
};

#endif // PUSHDIALOG_H
