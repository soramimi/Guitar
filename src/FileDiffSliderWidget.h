#ifndef FILEDIFFSLIDERWIDGET_H
#define FILEDIFFSLIDERWIDGET_H

#include <QWidget>
#include <QPixmap>

#include "FileDiffWidget.h"
#include "FilePreviewWidget.h"

class FileDiffSliderWidget : public QWidget
{
	Q_OBJECT
private:
	MainWindow *mainwindow;
	FileDiffWidget *file_diff_widget;
	DiffWidgetData const *diff_widget_data;
	bool visible = false;
	int scroll_total;
	int scroll_value;
	int scroll_visible_size;
	QPixmap left_pixmap;
	QPixmap right_pixmap;

	void scroll(int pos);
	void updatePixmap();
public:
	explicit FileDiffSliderWidget(QWidget *parent = 0);

	void imbue_(MainWindow *mw, FileDiffWidget *fdw, const DiffWidgetData *diff_widget_data)
	{
		this->mainwindow = mw;
		this->file_diff_widget = fdw;
		this->diff_widget_data = diff_widget_data;
	}

	void clear(bool v);
	void setScrollPos(int total, int value, int size);
protected:
	void paintEvent(QPaintEvent *);
	void resizeEvent(QResizeEvent *);
	void mousePressEvent(QMouseEvent *);
	void mouseMoveEvent(QMouseEvent *);
	void wheelEvent(QWheelEvent *event);
signals:
	void valueChanged(int value);
	void scrollByWheel(int lines);
public slots:
};

#endif // FILEDIFFSLIDERWIDGET_H
