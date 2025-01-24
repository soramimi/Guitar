#ifndef REPOSITORYPROPERTYDIALOG_H
#define REPOSITORYPROPERTYDIALOG_H

#include "RepositoryData.h"
#include "EditRemoteDialog.h"
#include <QDialog>
#include "Git.h"

class MainWindow;
class QTableWidget;
class QTableWidgetItem;

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
	
	void updateRemotesTable();
	bool execEditRemoteDialog(Git::Remote *remote, EditRemoteDialog::Operation op);
	Git::Remote selectedRemote() const;
	bool isNameEditMode() const;
	void setNameEditMode(bool f);
	void reflectRemotesTable();
public:
	explicit RepositoryPropertyDialog(MainWindow *parent, GitPtr g, RepositoryData const &item, bool open_repository_menu = false);
	~RepositoryPropertyDialog() override;

	bool isRemoteChanged() const;
	bool isNameChanged() const;
	QString getName();
private slots:
	void on_pushButton_edit_name_clicked();
	void on_pushButton_remote_add_clicked();
	void on_pushButton_remote_edit_clicked();
	void on_pushButton_remote_remove_clicked();
	void on_tableWidget_itemDoubleClicked(QTableWidgetItem *item);
public slots:
	void reject();
};

#endif // REPOSITORYPROPERTYDIALOG_H
