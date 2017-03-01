#ifndef LOGWIDGET_H
#define LOGWIDGET_H

#include "FileDiffWidget.h"

class MainWindow;

class LogWidget : public FileDiffWidget
{
	Q_OBJECT
public:
	explicit LogWidget(QWidget *parent = 0);

	void init(MainWindow *mw);
signals:

public slots:
};

#endif // LOGWIDGET_H
