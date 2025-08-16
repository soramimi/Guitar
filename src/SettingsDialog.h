#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include "ApplicationSettings.h"
#include "MainWindow.h"
#include <QDialog>

namespace Ui {
class SettingsDialog;
}

class QTreeWidgetItem;

class SettingsDialog : public QDialog
{
	Q_OBJECT
public:
	ApplicationSettings settings_;
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
		return settings_;
	}

private slots:
	void on_treeWidget_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
public slots:
	void accept() override;
	void done(int) override;
};

#endif // SETTINGSDIALOG_H
