#include "ClearButton.h"

#include <QPainter>

ClearButton::ClearButton(QWidget *parent)
	: QToolButton(parent)
{
	pixmap = QPixmap(":/image/clear.png");
}

void ClearButton::paintEvent(QPaintEvent * /*event*/)
{
	QPainter pr(this);
	if (underMouse()) {
		pr.setOpacity(1.0);
	} else {
		pr.setOpacity(0.5);
	}
	int w = pixmap.width();
	int h = pixmap.height();
	int x = (width() - w) / 2;
	int y = (height() - h) / 2;
	if (isDown()) {
		x++;
		y++;
	}
	pr.drawPixmap(x, y, w, h, pixmap);
}
