
#ifndef RESETANDCLEANDIALOG_H
#define RESETANDCLEANDIALOG_H

#include <QDialog>


namespace Ui { class ResetAndCleanDialog; }

class MainWindow;

class ResetAndCleanDialog : public QDialog
{
	Q_OBJECT

public:
	explicit ResetAndCleanDialog(QWidget *parent = nullptr);
	~ResetAndCleanDialog();
	void perform(MainWindow *mainwindow);
private:
	Ui::ResetAndCleanDialog *ui;
};

#endif // RESETANDCLEANDIALOG_H
