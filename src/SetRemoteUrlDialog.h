#ifndef SETREMOTEURLDIALOG_H
#define SETREMOTEURLDIALOG_H

#include <QDialog>
#include "Git.h"

class MainWindow;

namespace Ui {
class SetRemoteUrlDialog;
}

class SetRemoteUrlDialog : public QDialog
{
	Q_OBJECT
private:
	GitPtr g_;
	MainWindow *mainwindow = nullptr;

	GitPtr git();
public:
	explicit SetRemoteUrlDialog(QWidget *parent = 0);
	~SetRemoteUrlDialog();

	void exec(MainWindow *mainwindow, GitPtr g);
private:
	Ui::SetRemoteUrlDialog *ui;
	void updateRemotesTable();
public slots:
	void accept();
private slots:
	void on_pushButton_test_clicked();
};

#endif // SETREMOTEURLDIALOG_H
