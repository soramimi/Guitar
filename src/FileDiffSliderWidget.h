#ifndef FILEDIFFSLIDERWIDGET_H
#define FILEDIFFSLIDERWIDGET_H

#include <QWidget>
#include <QPixmap>

#include "FileDiffWidget.h"

class FileDiffSliderWidget : public QWidget
{
	Q_OBJECT
private:
	bool visible = false;
	int scroll_total;
	int scroll_value;
	int scroll_visible_size;
	FileDiffWidget *fdw = nullptr;
	QPixmap left_pixmap;
	QPixmap right_pixmap;
	void scroll(int pos);
public:
	explicit FileDiffSliderWidget(QWidget *parent = 0);

	void setFileDiffWidget(FileDiffWidget *w)
	{
		fdw = w;
	}

	void updatePixmap();

	void clear(bool v);
	void setScrollPos(int total, int value, int size);
signals:
	void valueChanged(int value);
public slots:

	// QWidget interface
protected:
	void paintEvent(QPaintEvent *);

	// QWidget interface
protected:
	void resizeEvent(QResizeEvent *);

	// QWidget interface
protected:
	void mousePressEvent(QMouseEvent *);

	// QWidget interface
protected:
	void mouseMoveEvent(QMouseEvent *);
};

#endif // FILEDIFFSLIDERWIDGET_H
