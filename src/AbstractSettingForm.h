#ifndef ABSTRACTSETTINGFORM_H
#define ABSTRACTSETTINGFORM_H

#include "SettingsDialog.h"
#include <QWidget>

class ApplicationSettings;
class MainWindow;

class AbstractSettingForm : public QWidget {
	Q_OBJECT
private:
	MainWindow *mainwindow_ = nullptr;
	ApplicationSettings *settings_ = nullptr;
protected:
	MainWindow *mainwindow();
	ApplicationSettings *settings();
public:
	AbstractSettingForm(QWidget *parent = nullptr);
	void reset(MainWindow *mw, ApplicationSettings *s)
	{
		mainwindow_ = mw;
		settings_ = s;
	}
	virtual void exchange(bool save) = 0;
};

#endif // ABSTRACTSETTINGFORM_H
