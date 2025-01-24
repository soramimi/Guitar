#ifndef COMMITLOGTABLEWIDGET_H
#define COMMITLOGTABLEWIDGET_H

#include <QTableWidget>

class MainWindow;

/**
 * @brief コミットログテーブルウィジェット
 */
class CommitLogTableWidget : public QTableWidget {
	Q_OBJECT
	friend class CommitLogTableWidgetDelegate;
private:
	MainWindow *mainwindow_ = nullptr;
	MainWindow *mainwindow() { return mainwindow_; }

public:
	explicit CommitLogTableWidget(QWidget *parent = nullptr);
	void setup(MainWindow *frame);
protected:
	void paintEvent(QPaintEvent *) override;
	void resizeEvent(QResizeEvent *e) override;
protected slots:
	void verticalScrollbarValueChanged(int value) override;
};

#endif // COMMITLOGTABLEWIDGET_H
