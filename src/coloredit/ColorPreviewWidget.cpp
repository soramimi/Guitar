#include "ColorPreviewWidget.h"

#include <QPainter>

ColorPreviewWidget::ColorPreviewWidget(QWidget *parent)
	: QWidget(parent)
{
	color_ = Qt::red;
}

void ColorPreviewWidget::setColor(const QColor &color)
{
	color_ = color;
	update();
}

void ColorPreviewWidget::paintEvent(QPaintEvent *)
{
	int w = width();
	int h = height();
	QPainter pr(this);
	pr.fillRect(0, 0, w - 1, 1, Qt::black);
	pr.fillRect(0, 1, 1, h - 1, Qt::black);
	pr.fillRect(w - 1, 0, 1, h - 1, Qt::black);
	pr.fillRect(1, h - 1, w - 1, 1, Qt::black);
	pr.fillRect(1, 1, w - 2, h - 2, color_);
}
