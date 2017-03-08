#include "HyperLinkLabel.h"
#include <QApplication>
#include <QPainter>
#include <QStyleOption>

HyperLinkLabel::HyperLinkLabel(QWidget *parent) :
	QLabel(parent)
{
	QFont f = font();
	f.setUnderline(true);
	setFont(f);

	setCursor(Qt::PointingHandCursor);
}

void HyperLinkLabel::paintEvent(QPaintEvent *)
{
	QPainter painter(this);

	QStyleOption opt;
	opt.initFrom(this);
	opt.palette.setColor(QPalette::WindowText, Qt::blue);

	int flags = QStyle::visualAlignment(QApplication::layoutDirection(), alignment());
	flags |= Qt::TextHideMnemonic;

	QRect r = contentsRect();
	QStyle *style = QWidget::style();

	style->drawItemText(&painter, r, flags, opt.palette, isEnabled(), text(), foregroundRole());
}


void HyperLinkLabel::mousePressEvent(QMouseEvent *)
{
	grabMouse();
}

void HyperLinkLabel::mouseReleaseEvent(QMouseEvent *)
{
	releaseMouse();

	QPoint pos = QCursor::pos();
	pos = mapFromGlobal(pos);

	QRect r(0, 0, width(), height());

	if (r.contains(pos)) {
		emit clicked();
	}
}

