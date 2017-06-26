#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include "MainWindow.h"
#include "main.h"

namespace Ui {
class SettingsDialog;
}

class QTreeWidgetItem;

class SettingsDialog : public QDialog
{
	Q_OBJECT
public:
	ApplicationSettings set;
private:
	MainWindow *mainwindow_;

	void loadSettings();
	void saveSettings();

public:
	explicit SettingsDialog(MainWindow *parent);
	~SettingsDialog();

	MainWindow *mainwindow()
	{
		return mainwindow_;
	}

	SettingsDialog *dialog()
	{
		return this;
	}

	ApplicationSettings const &settings() const
	{
		return set;
	}

	static void loadSettings(ApplicationSettings *set);
private slots:

	void on_treeWidget_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);

private:
	Ui::SettingsDialog *ui;

	// QDialog interface
public slots:
	void accept();
};

#endif // SETTINGSDIALOG_H
