#ifndef WELCOMEWIZARDDIALOG_H
#define WELCOMEWIZARDDIALOG_H

#include "AvatarLoader.h"

#include <QDialog>

class MainWindow;

namespace Ui {
class WelcomeWizardDialog;
}

class WelcomeWizardDialog : public QDialog {
	Q_OBJECT
private:
private:
	Ui::WelcomeWizardDialog *ui;
	void setAvatar(const QImage &icon);

	MainWindow *mainwindow_;
	QList<QWidget *> pages_;
	QString default_branch_name_;
	void updateUI();
public:
	explicit WelcomeWizardDialog(MainWindow *parent = nullptr);
	~WelcomeWizardDialog() override;

	void set_user_name(QString const &v);
	void set_user_email(QString const &v);
	void set_default_working_folder(QString const &v);
	void set_git_command_path(QString const &v);

	QString user_name() const;
	QString user_email() const;
	QString default_working_folder() const;
	QString git_command_path() const;

	QString default_branch_name() const;
private slots:
	void avatarReady();
	void on_stackedWidget_currentChanged(int arg1);
	void on_pushButton_browse_default_workiing_folder_clicked();
	void on_pushButton_browse_git_clicked();
	void on_pushButton_get_icon_clicked();
	void on_pushButton_prev_clicked();
	void on_pushButton_next_clicked();
	void on_lineEdit_git_textChanged(const QString &arg1);
	void on_lineEdit_default_branch_name_textChanged(const QString &arg1);
	void on_radioButton_branch_main_clicked();
	void on_radioButton_branch_master_clicked();
	void on_radioButton_branch_unset_clicked();
	void on_radioButton_branch_other_clicked();
};

#endif // WELCOMEWIZARDDIALOG_H
