#include "RepositoryInfoFrame.h"
#include <QPainter>
#include "ApplicationGlobal.h"

RepositoryInfoFrame::RepositoryInfoFrame(QWidget *parent)
	: QFrame(parent)
{

}

void RepositoryInfoFrame::paintEvent(QPaintEvent *)
{
	double x = 1;
	double y = 1;
	double w = width() - 2;
	double h = height() - 2;
	QPainter pr(this);
	pr.setPen(QPen(palette().color(QPalette::Dark), 2));
	pr.setBrush(palette().color(QPalette::Light));
	pr.setRenderHint(QPainter::Antialiasing);
	pr.drawRoundedRect(QRectF(x, y, w, h), 5, 5);
}
