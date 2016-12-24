#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include "MainWindow.h"
#include "main.h"

namespace Ui {
class SettingsDialog;
}

class SettingsDialog : public QDialog
{
	Q_OBJECT
public:
	ApplicationSettings set;
private:
	MainWindow *mainwindow;

	void loadSettings();
	void saveSettings();

public:
	explicit SettingsDialog(MainWindow *parent);
	~SettingsDialog();

	ApplicationSettings const &settings() const
	{
		return set;
	}

	static void loadSettings(ApplicationSettings *set);
private slots:
	void on_pushButton_select_git_command_clicked();

private:
	Ui::SettingsDialog *ui;

	// QDialog interface
public slots:
	void accept();
};

#endif // SETTINGSDIALOG_H
