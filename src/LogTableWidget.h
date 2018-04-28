#ifndef LOGTABLEWIDGET_H
#define LOGTABLEWIDGET_H

#include <QTableWidget>

class MainWindow;
class LogTableWidgetDelegate;

class LogTableWidget : public QTableWidget
{
	Q_OBJECT
	friend class LogTableWidgetDelegate;
private:
	struct Private;
	Private *m;
	MainWindow *mainwindow();
public:
	explicit LogTableWidget(QWidget *parent = 0);
	~LogTableWidget();
protected:
	void paintEvent(QPaintEvent *);
};

#endif // LOGTABLEWIDGET_H
