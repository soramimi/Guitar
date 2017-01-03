#include "FileDiffSliderWidget.h"
#include "MainWindow.h"

#include <QPainter>
#include <QMouseEvent>

FileDiffSliderWidget::FileDiffSliderWidget(QWidget *parent)
	: QWidget(parent)
{

}

void FileDiffSliderWidget::updatePixmap()
{
	MainWindow *mw = qobject_cast<MainWindow *>(window());
	Q_ASSERT(mw);
	left_pixmap = mw->makeDiffPixmap(ViewType::Left, 8, height());
	right_pixmap = mw->makeDiffPixmap(ViewType::Right, 8, height());
}

void FileDiffSliderWidget::paintEvent(QPaintEvent *)
{
	if (!visible) return;
	if (scroll_total < 1) return;

	if (left_pixmap.isNull() || right_pixmap.isNull()) {
		updatePixmap();
	}

	int w;
	w = left_pixmap.width();
	QPainter pr(this);
	pr.fillRect(w, 0, 4, height(), QColor(240, 240, 240));
	pr.drawPixmap(0, 0, left_pixmap);
	pr.drawPixmap(w + 4, 0, right_pixmap);

	int y = scroll_value * height() / scroll_total;
	int h = (scroll_value + scroll_visible_size) * height() / scroll_total;
	h -= y;
	if (h < 1) h = 1;
	pr.fillRect(w + 1, y, 2, h, Qt::black);
}

void FileDiffSliderWidget::resizeEvent(QResizeEvent *)
{
	clear(visible);
}

void FileDiffSliderWidget::scroll(int pos)
{
	int v = pos;
	v = v * scroll_total / height() - scroll_visible_size / 2;
	if (v < 0) v = 0;

	emit valueChanged(v);
}

void FileDiffSliderWidget::mousePressEvent(QMouseEvent *e)
{
	if (visible && e->button() == Qt::LeftButton) {
		scroll(e->pos().y());
	}
}

void FileDiffSliderWidget::mouseMoveEvent(QMouseEvent *e)
{
	if (visible && (e->buttons() & Qt::LeftButton)) {
		scroll(e->pos().y());
	}
}

void FileDiffSliderWidget::clear(bool v)
{
	left_pixmap = QPixmap();
	right_pixmap = QPixmap();
	visible = v;
	update();
}

void FileDiffSliderWidget::setScrollPos(int total, int value, int size)
{
	scroll_total = total;
	scroll_value = value;
	scroll_visible_size = size;
	visible = (scroll_total > 0) && (scroll_visible_size > 0);
	update();
}
