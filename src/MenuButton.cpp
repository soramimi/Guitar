#include "MenuButton.h"
#include <QPainter>
#include "Theme.h"
#include "ApplicationGlobal.h"

MenuButton::MenuButton(QWidget *parent)
	: QToolButton(parent)
{
	pixmap = global->theme->resource_menu_png();
}

void MenuButton::paintEvent(QPaintEvent *event)
{
	QPainter pr(this);
	int w = pixmap.width();
	int h = pixmap.height();
	int x = (width() - w) / 2;
	int y = (height() - h) / 2;
	pr.drawPixmap(x, y, w, h, pixmap);
}
