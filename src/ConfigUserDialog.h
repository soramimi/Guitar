#ifndef CONFIGUSERDIALOG_H
#define CONFIGUSERDIALOG_H

#include "Git.h"

#include <QDialog>

class MainWindow;

namespace Ui {
class ConfigUserDialog;
}

class ConfigUserDialog : public QDialog {
	Q_OBJECT
private:
	Ui::ConfigUserDialog *ui;
	struct Private;
	Private *m;

	MainWindow *mainwindow();
	QString email() const;
	void updateAvatar(const QString &email, bool request);
public:
	explicit ConfigUserDialog(MainWindow *parent, Git::User const &global_user, Git::User const &local_user, QString const &repo);
	~ConfigUserDialog() override;

	Git::User user(bool global) const;
protected:
	void customEvent(QEvent *event);
private slots:
	void on_pushButton_get_icon_clicked();
	void on_lineEdit_global_name_textChanged(const QString &text);
	void on_lineEdit_global_email_textEdited(const QString &text);
	void on_lineEdit_local_name_textEdited(const QString &text);
	void on_lineEdit_local_email_textEdited(const QString &text);
	void on_checkBox_unset_local_stateChanged(int arg1);
};

#endif // CONFIGUSERDIALOG_H
