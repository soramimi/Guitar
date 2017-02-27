#ifndef SETUSERDIALOG_H
#define SETUSERDIALOG_H

#include "Git.h"

#include <QDialog>

namespace Ui {
class SetUserDialog;
}

class SetUserDialog : public QDialog
{
	Q_OBJECT
private:
	Git::User global_user;
	Git::User repo_user;
public:
	explicit SetUserDialog(QWidget *parent, Git::User global_user, Git::User repo_user, const QString &repo);
	~SetUserDialog();

	bool isGlobalChecked() const;
	bool isRepositoryChecked() const;

	Git::User user() const;
private slots:
	void on_radioButton_global_toggled(bool checked);

	void on_radioButton_repository_toggled(bool checked);

	void on_lineEdit_name_textChanged(const QString &text);

	void on_lineEdit_mail_textChanged(const QString &text);

private:
	Ui::SetUserDialog *ui;
};

#endif // SETUSERDIALOG_H
