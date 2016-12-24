#include "RepositoryInfoFrame.h"

#include <QPainter>

RepositoryInfoFrame::RepositoryInfoFrame(QWidget *parent)
	: QFrame(parent)
{

}

void RepositoryInfoFrame::paintEvent(QPaintEvent *event)
{
	double x = 1;
	double y = 1;
	double w = width() - 2;
	double h = height() - 2;
	QPainter pr(this);
	pr.setPen(QPen(QColor(192, 192, 192), 2));
	pr.setBrush(Qt::white);
	pr.setRenderHint(QPainter::Antialiasing);
	pr.drawRoundedRect(QRectF(x, y, w, h), 5, 5);
}
