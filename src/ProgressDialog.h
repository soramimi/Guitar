#ifndef PROGRESSDIALOG_H
#define PROGRESSDIALOG_H

#include <QDialog>

namespace Ui {
class ProgressDialog;
}

class MainWindow;

class ProgressDialog : public QDialog
{
	Q_OBJECT
private:
	int interrupt_counter = 5;
	bool canceled_by_user = false;
	void updateInterruptCounter();
public:
	explicit ProgressDialog(MainWindow *parent = 0);
	~ProgressDialog();

	void setLabelText(const QString &text);
	bool canceledByUser() const;


private:
	Ui::ProgressDialog *ui;

public slots:
	void reject();
	void interrupt();
private slots:
	void writeLog_(QString ba);
protected:
	void keyPressEvent(QKeyEvent *event);
signals:
	void finish();
	void writeLog(QString text);
};

#endif // PROGRESSDIALOG_H
