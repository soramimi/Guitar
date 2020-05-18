#ifndef REPOSITORYPROPERTYDIALOG_H
#define REPOSITORYPROPERTYDIALOG_H

#include "RepositoryData.h"
#include "BasicRepositoryDialog.h"
#include "EditRemoteDialog.h"
#include <QDialog>
#include "Git.h"

class BasicMainWindow;

namespace Ui {
class RepositoryPropertyDialog;
}

class RepositoryPropertyDialog : public BasicRepositoryDialog {
	Q_OBJECT
private:
	Ui::RepositoryPropertyDialog *ui;
	RepositoryItem repository;
	bool remote_changed = false;
	Git::Context const *gcx;
	void updateRemotesTable();
	bool execEditRemoteDialog(Git::Remote *remote, EditRemoteDialog::Operation op);
	Git::Remote selectedRemote() const;
	void toggleRemoteMenuActivity();
public:
	explicit RepositoryPropertyDialog(BasicMainWindow *parent, const Git::Context *gcx, const GitPtr &g, RepositoryItem const &item, bool open_repository_menu = false);
	~RepositoryPropertyDialog() override;

	bool isRemoteChanged() const;
private slots:
	void on_pushButton_remote_add_clicked();
	void on_pushButton_remote_edit_clicked();
	void on_pushButton_remote_remove_clicked();
	void on_pushButton_remote_menu_clicked();
};

#endif // REPOSITORYPROPERTYDIALOG_H
