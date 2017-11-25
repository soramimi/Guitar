#ifndef FILEDIFFSLIDERWIDGET_H
#define FILEDIFFSLIDERWIDGET_H

#include "MainWindow.h"

#include <QWidget>
#include <QPixmap>

#include "FileDiffWidget.h"
//#include "ImageViewWidget.h"


class FileDiffSliderWidget : public QWidget
{
	Q_OBJECT
private:
	struct Private;
	Private *m;

	void scroll_(int pos);
	QPixmap makeDiffPixmap(FileDiffWidget::Pane pane, int width, int height);
	void setValue(int v);
	void internalSetValue(int v);
public:
	explicit FileDiffSliderWidget(QWidget *parent = 0);
	~FileDiffSliderWidget();

//	void bind(MainWindow *mw, FileDiffWidget *fdw, const FileDiffWidget::DiffData *diffdata, const FileDiffWidget::DrawData *drawdata);

	void clear(bool v);
	void setScrollPos(int total, int value, int size);
	void bind(FileDiffWidget *w);
	void updatePixmap();
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
