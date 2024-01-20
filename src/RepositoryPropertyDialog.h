#ifndef REPOSITORYPROPERTYDIALOG_H
#define REPOSITORYPROPERTYDIALOG_H

#include "RepositoryData.h"
#include "BasicRepositoryDialog.h"
#include "EditRemoteDialog.h"
#include <QDialog>
#include "Git.h"

class MainWindow;

namespace Ui {
class RepositoryPropertyDialog;
}

class RepositoryPropertyDialog : public BasicRepositoryDialog {
	Q_OBJECT
private:
	Ui::RepositoryPropertyDialog *ui;
	RepositoryData repository;
	bool remote_changed = false;
	bool name_changed = false;
	Git::Context const *gcx;
	void updateRemotesTable();
	bool execEditRemoteDialog(Git::Remote *remote, EditRemoteDialog::Operation op);
	Git::Remote selectedRemote() const;
	void toggleRemoteMenuActivity();
	bool isNameEditMode() const;
	void setNameEditMode(bool f);
public:
	explicit RepositoryPropertyDialog(MainWindow *parent, const Git::Context *gcx, GitPtr g, RepositoryData const &item, bool open_repository_menu = false);
	~RepositoryPropertyDialog() override;

	bool isRemoteChanged() const;
	bool isNameChanged() const;
	QString getName();
private slots:
	void on_pushButton_remote_add_clicked();
	void on_pushButton_remote_edit_clicked();
	void on_pushButton_remote_remove_clicked();
	void on_pushButton_remote_menu_clicked();
	void on_pushButton_edit_name_clicked();

	// QDialog interface
public slots:
	void reject();
};

#endif // REPOSITORYPROPERTYDIALOG_H
