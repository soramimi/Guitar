#ifndef SETREMOTEURLDIALOG_H
#define SETREMOTEURLDIALOG_H

#include <QDialog>
#include "Git.h"
#include "RepositoryPropertyDialog.h"

class MainWindow;

namespace Ui {
class SetRemoteUrlDialog;
}

class SetRemoteUrlDialog : public BasicRepositoryDialog
{
	Q_OBJECT
private:

public:
	explicit SetRemoteUrlDialog(MainWindow *mainwindow, GitPtr g);
	~SetRemoteUrlDialog();

	int exec();
private:
	Ui::SetRemoteUrlDialog *ui;
	void updateRemotesTable();
//	void updateRemotesTable_(QTableWidget *tablewidget);
public slots:
	void accept();
private slots:
	void on_pushButton_test_clicked();
};

#endif // SETREMOTEURLDIALOG_H
