#ifndef LOGTABLEWIDGET_H
#define LOGTABLEWIDGET_H

#include <QTableWidget>

class MainWindow;
class LogTableWidgetDelegate;

class LogTableWidget : public QTableWidget {
	Q_OBJECT
	friend class LogTableWidgetDelegate;
private:
	struct Private;
	Private *m;
	MainWindow *mainwindow();
public:
	explicit LogTableWidget(QWidget *parent = nullptr);
	~LogTableWidget() override;
protected:
	void paintEvent(QPaintEvent *) override;
	void resizeEvent(QResizeEvent *e) override;
protected slots:
	void verticalScrollbarValueChanged(int value) override;
};

#endif // LOGTABLEWIDGET_H
