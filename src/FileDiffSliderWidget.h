#ifndef FILEDIFFSLIDERWIDGET_H
#define FILEDIFFSLIDERWIDGET_H

#include "MainWindow.h"

#include <QWidget>
#include <QPixmap>
#include <functional>

#include "FileDiffWidget.h"

typedef std::function<QPixmap(FileDiffWidget::Pane pane, int width, int height)> fn_pixmap_maker_t;

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

	void clear(bool v);
	void setScrollPos(int total, int value, int size);
	void bind(FileDiffWidget *w, fn_pixmap_maker_t pixmap_maker);
	void updatePixmap();
	static QPixmap makeDiffPixmap(FileDiffWidget::Pane pane, int width, int height, const TextDiffLineList &left_lines, const TextDiffLineList &right_lines);
protected:
	void paintEvent(QPaintEvent *);
	void resizeEvent(QResizeEvent *);
	void mousePressEvent(QMouseEvent *);
	void mouseMoveEvent(QMouseEvent *);
	void wheelEvent(QWheelEvent *event);
signals:
	void valueChanged(int value);
	void scrollByWheel(int lines);
};

#endif // FILEDIFFSLIDERWIDGET_H
