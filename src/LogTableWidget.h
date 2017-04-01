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
public:
	explicit LogTableWidget(QWidget *parent = 0);
	~LogTableWidget();

signals:

public slots:

	// QObject interface
public:
	bool event(QEvent *);

	// QWidget interface
protected:
	void paintEvent(QPaintEvent *);
};

#endif // LOGTABLEWIDGET_H
