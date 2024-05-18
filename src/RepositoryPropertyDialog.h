#ifndef REPOSITORYPROPERTYDIALOG_H
#define REPOSITORYPROPERTYDIALOG_H

#include "RepositoryData.h"
#include "EditRemoteDialog.h"
#include <QDialog>
#include "Git.h"

class MainWindow;
class QTableWidget;

namespace Ui {
class RepositoryPropertyDialog;
}

class RepositoryPropertyDialog : public QDialog {
	Q_OBJECT
private:
	Ui::RepositoryPropertyDialog *ui;
	struct Private;
	Private *m;
	
	MainWindow *mainwindow();
	
	GitPtr git();
	
	const std::vector<Git::Remote> *remotes() const;
	void getRemotes_();
	void setSshKey_(const QString &sshkey);
	
	
	
	RepositoryData repository;
	bool remote_changed = false;
	bool name_changed = false;
	Git::Context const *gcx;
	void updateRemotesTable();
	bool execEditRemoteDialog(Git::Remote *remote, EditRemoteDialog::Operation op);
	Git::Remote selectedRemote() const;
	bool isNameEditMode() const;
	void setNameEditMode(bool f);
	void reflectRemotesTable();
public:
	explicit RepositoryPropertyDialog(MainWindow *parent, const Git::Context *gcx, GitPtr g, RepositoryData const &item, bool open_repository_menu = false);
	~RepositoryPropertyDialog() override;

	bool isRemoteChanged() const;
	bool isNameChanged() const;
	QString getName();
private slots:
	void on_pushButton_remote_add_clicked();
	// void on_pushButton_remote_edit_clicked();
	void on_pushButton_remote_remove_clicked();
	void on_pushButton_edit_name_clicked();

	
public slots:
	void reject();
};

#endif // REPOSITORYPROPERTYDIALOG_H
