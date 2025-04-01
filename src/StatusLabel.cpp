#include "StatusLabel.h"

StatusLabel::StatusLabel(QWidget *parent)
	: QLabel(parent)
{
	setTextFormat(Qt::PlainText);
}

QSize StatusLabel::minimumSizeHint() const
{
	QSize sz = QLabel::minimumSizeHint();
	sz.rwidth() = 0; // テキストが長くなっても横幅を広げない
	return sz;
}
