#include "MaximizeButton.h"
#include <QPainter>
#include <QIcon>

MaximizeButton::MaximizeButton(QWidget *parent)
	: QToolButton(parent)
{
	pixmap = QPixmap(":/image/maximize.png");
}

void MaximizeButton::paintEvent(QPaintEvent * /*event*/)
{
	QPainter pr(this);
	int w = pixmap.width();
	int h = pixmap.height();
	int x = (width() - w) / 2;
	int y = (height() - h) / 2;
	pr.drawPixmap(x, y, w, h, pixmap);
}
