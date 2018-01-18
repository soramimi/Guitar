#include "ClearButton.h"
#include "ApplicationGlobal.h"
#include "Theme.h"

#include <QPainter>

ClearButton::ClearButton(QWidget *parent)
	: QToolButton(parent)
{
	pixmap = global->theme->resource_clear_png();
}

void ClearButton::paintEvent(QPaintEvent * /*event*/)
{
	QPainter pr(this);
	pr.setOpacity(underMouse() ? 1.0 : 0.5);
	int w = pixmap.width();
	int h = pixmap.height();
	int x = (width() - w) / 2;
	int y = (height() - h) / 2;
	int delta = isDown() ? 1 : 0;
	pr.drawPixmap(x + delta, y + delta, w, h, pixmap);
}
