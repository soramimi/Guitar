#include "ClearButton.h"
#include "ApplicationGlobal.h"
#include "Theme.h"

#include <QPainter>

ClearButton::ClearButton(QWidget *parent)
	: QToolButton(parent)
{
	icon_ = global->theme->resource_clear_icon();
}

void ClearButton::paintEvent(QPaintEvent * /*event*/)
{
	QPainter pr(this);
	pr.setRenderHint(QPainter::Antialiasing);
	pr.setOpacity(underMouse() ? 1.0 : 0.5);
	QRect r = rect();
	int n = std::min(r.width(), r.height());
	r = {1, 1, n - 2, n - 2};
	if (isDown()) {
		r.translate(1, 1);
	}
	icon_.paint(&pr, r);
}
