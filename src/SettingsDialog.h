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
	Ui::SettingsDialog *ui;
	MainWindow *mainwindow_;

	void exchange(bool save);

	void loadSettings();
	void saveSettings();

public:
	explicit SettingsDialog(MainWindow *parent);
	~SettingsDialog() override;

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

	static void loadSettings(ApplicationSettings *as);
	static void saveSettings(const ApplicationSettings *as);
private slots:
	void on_treeWidget_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
public slots:
	void accept() override;
	void done(int) override;
};

#endif // SETTINGSDIALOG_H
