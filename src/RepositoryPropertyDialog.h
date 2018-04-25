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
public:
	explicit RepositoryPropertyDialog(MainWindow *parent, GitPtr g, RepositoryItem const &item);
	~RepositoryPropertyDialog();

private slots:

	void on_label_remote_menu_linkActivated(const QString &link);

	void on_pushButton_remote_add_clicked();

	void on_pushButton_remote_edit_clicked();

	void on_pushButton_remote_remove_clicked();

private:
	Ui::RepositoryPropertyDialog *ui;
	void updateRemotesTable();
	bool execEditRemoteDialog(Git::Remote *remote, EditRemoteDialog::Operation op);
	Git::Remote selectedRemote() const;
};

#endif // REPOSITORYPROPERTYDIALOG_H
