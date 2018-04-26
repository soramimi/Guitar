#ifndef REPOSITORYPROPERTYDIALOG_H
#define REPOSITORYPROPERTYDIALOG_H

#include "RepositoryData.h"
#include "BasicRepositoryDialog.h"
#include "EditRemoteDialog.h"
#include <QDialog>
#include "Git.h"


namespace Ui {
class RepositoryPropertyDialog;
}

class RepositoryPropertyDialog : public BasicRepositoryDialog
{
	Q_OBJECT
private:
	RepositoryItem repository;
	bool remote_changed = false;
public:
	explicit RepositoryPropertyDialog(MainWindow *parent, GitPtr g, RepositoryItem const &item, bool open_repository_menu = false);
	~RepositoryPropertyDialog();

	bool isRemoteChanged() const;
private slots:
	void on_pushButton_remote_add_clicked();
	void on_pushButton_remote_edit_clicked();
	void on_pushButton_remote_remove_clicked();
	void on_pushButton_remote_menu_clicked();
private:
	Ui::RepositoryPropertyDialog *ui;
	void updateRemotesTable();
	bool execEditRemoteDialog(Git::Remote *remote, EditRemoteDialog::Operation op);
	Git::Remote selectedRemote() const;
	void toggleRemoteMenuActivity();
};

#endif // REPOSITORYPROPERTYDIALOG_H
