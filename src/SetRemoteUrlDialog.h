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
	Ui::SetRemoteUrlDialog *ui;
	QStringList remotes;
	void updateRemotesTable();
public:
	explicit SetRemoteUrlDialog(MainWindow *mainwindow, QStringList const &remotes, const GitPtr &g);
	~SetRemoteUrlDialog() override;

	int exec() override;
private slots:
	void on_pushButton_test_clicked();
public slots:
	void accept() override;
};

#endif // SETREMOTEURLDIALOG_H
