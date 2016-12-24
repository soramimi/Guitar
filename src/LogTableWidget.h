#ifndef LOGTABLEWIDGET_H
#define LOGTABLEWIDGET_H

#include <QTableWidget>

class MainWindow;
class MyItemDelegate;

class LogTableWidget : public QTableWidget
{
	Q_OBJECT
	friend class MyItemDelegate;
private:
	struct Private;
	Private *pv;
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
