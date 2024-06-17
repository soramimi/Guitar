#include "ProgressTextLabel.h"

#include <QPainter>
#include <QPainterPath>

ProgressTextLabel::ProgressTextLabel(QWidget *parent)
	: QLabel(parent)
{
	timer_.start(30);
	connect(&timer_, &QTimer::timeout, [this]() {
		animation_ = (animation_ + 1) % 16;
		update();
	});

}

void ProgressTextLabel::setProgress(float progress)
{
	progress_ = progress;
}

void ProgressTextLabel::paintEvent(QPaintEvent *event)
{
	QPainter pr(this);
	QRect rect(0, height() - 10, width(), 6);
	pr.fillRect(rect, Qt::black);
	rect.adjust(1, 1, -1, -1);
	pr.setClipRect(rect);
	if (progress_ > 0) {
		float progress = std::min(progress_, 1.0f);
		rect.setRight(rect.left() + rect.width() * progress);
		pr.fillRect(rect, Qt::green);
	} else if (progress_ < 0) {
		QBrush br0(Qt::cyan);
		QBrush br1(Qt::black);
		int x = animation_ - 16;
		bool color = false;
		while (x < width()) {
			QPainterPath path;
			int px = x;
			int py = rect.bottom();
			path.moveTo(px, py);
			px += 8;
			py -= 8;
			path.lineTo(px, py);
			px += 8;
			path.lineTo(px, py);
			px -= 8;
			py += 8;
			path.lineTo(px, py);
			px -= 8;
			path.lineTo(px, py);
			pr.fillPath(path, color ? br1 : br0);
			color = !color;
			x += 8;
		}
	}

	QLabel::paintEvent(event);
}
