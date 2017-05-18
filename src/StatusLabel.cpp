#include "StatusLabel.h"

StatusLabel::StatusLabel(QWidget *parent)
	: QLabel(parent)
{
}

QSize StatusLabel::minimumSizeHint() const
{
	QSize sz = QLabel::minimumSizeHint();
	sz.rwidth() = 0;
	return sz;
}
