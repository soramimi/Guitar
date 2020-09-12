#ifndef LOGTABLEWIDGET_H
#define LOGTABLEWIDGET_H

#include <QTableWidget>

class RepositoryWrapperFrame;
class LogTableWidgetDelegate;

/**
 * @brief コミットログテーブルウィジェット
 */
class LogTableWidget : public QTableWidget {
	Q_OBJECT
	friend class LogTableWidgetDelegate;
private:
	struct Private;
	Private *m;
	RepositoryWrapperFrame *frame();
public:
	explicit LogTableWidget(QWidget *parent = nullptr);
	~LogTableWidget() override;
	void bind(RepositoryWrapperFrame *frame);
protected:
	void paintEvent(QPaintEvent *) override;
	void resizeEvent(QResizeEvent *e) override;
protected slots:
	void verticalScrollbarValueChanged(int value) override;
};

#endif // LOGTABLEWIDGET_H
