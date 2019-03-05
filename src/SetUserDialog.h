#ifndef SETUSERDIALOG_H
#define SETUSERDIALOG_H

#include "Git.h"

#include <QDialog>

class BasicMainWindow;

namespace Ui {
class SetUserDialog;
}

class SetUserDialog : public QDialog {
	Q_OBJECT
private:
	Ui::SetUserDialog *ui;
	struct Private;
	Private *m;

	void setAvatar(const QIcon &icon);
	BasicMainWindow *mainwindow();
public:
	explicit SetUserDialog(BasicMainWindow *parent, Git::User const &global_user, Git::User const &repo_user, QString const &repo);
	~SetUserDialog() override;

	bool isGlobalChecked() const;
	bool isRepositoryChecked() const;

	Git::User user() const;
private slots:
	void on_radioButton_global_toggled(bool checked);
	void on_radioButton_repository_toggled(bool checked);
	void on_lineEdit_name_textChanged(QString const &text);
	void on_lineEdit_mail_textChanged(QString const &text);
	void on_pushButton_get_icon_clicked();
};

#endif // SETUSERDIALOG_H
