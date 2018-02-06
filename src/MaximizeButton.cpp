#include "MaximizeButton.h"
#include <QPainter>
#include <QIcon>
#include "Theme.h"
#include "ApplicationGlobal.h"

MaximizeButton::MaximizeButton(QWidget *parent)
	: QToolButton(parent)
{
	pixmap = global->theme->resource_maximize_png();
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
