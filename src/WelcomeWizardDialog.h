#ifndef WELCOMEWIZARDDIALOG_H
#define WELCOMEWIZARDDIALOG_H

#include "AvatarLoader.h"

#include <QDialog>

class MainWindow;

namespace Ui {
class WelcomeWizardDialog;
}

class WelcomeWizardDialog : public QDialog
{
	Q_OBJECT
private:
	MainWindow *mainwindow_;
	AvatarLoader avatar_loader_;
	QList<QWidget *> pages_;
public:
	explicit WelcomeWizardDialog(MainWindow *parent = 0);
	~WelcomeWizardDialog();

	void set_user_name(QString const &v);
	void set_user_email(QString const &v);
	void set_default_working_folder(QString const &v);
	void set_git_command_path(QString const &v);
	void set_file_command_path(QString const &v);

	QString user_name() const;
	QString user_email() const;
	QString default_working_folder() const;
	QString git_command_path() const;
	QString file_command_path() const;
private slots:
	void on_stackedWidget_currentChanged(int arg1);
	void on_pushButton_browse_default_workiing_folder_clicked();
	void on_pushButton_browse_git_clicked();
	void on_pushButton_browse_file_clicked();
	void on_pushButton_get_icon_clicked();
	void on_pushButton_prev_clicked();

	void on_pushButton_next_clicked();

private:
	Ui::WelcomeWizardDialog *ui;
	void setAvatar(const QIcon &icon);
};

#endif // WELCOMEWIZARDDIALOG_H
