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

	// generate stripe pattern brush
	{
		QImage image(16, 16, QImage::Format_RGB888);
		for (int y = 0; y < 16; y++) {
			uint32_t pattern = 0x00ff00ff;
			uint8_t *p = image.scanLine(y);
			for (int x = 0; x < 16; x++) {
				uint8_t r, g, b;
				r = g = b = 0;
				if ((pattern << (y + x)) & 0x80000000) {
					g = b = 0xff;
				}
				p[3 * x + 0] = r;
				p[3 * x + 1] = g;
				p[3 * x + 2] = b;
			}
		}
		pattern_brush_ = QBrush(image);
	}
}

void ProgressTextLabel::setElementVisible(bool bar, bool msg)
{
	bar_visible = bar;
	msg_visible = msg;
}

void ProgressTextLabel::setProgress(float progress)
{
	progress_ = progress;
}

void ProgressTextLabel::paintEvent(QPaintEvent *event)
{
	if (bar_visible) {
		QPainter pr(this);
		QRect rect(0, height() - 10, width(), 6);
		pr.fillRect(rect, Qt::black);
		rect.adjust(1, 1, -1, -1);
		pr.setClipRect(rect);
		if (progress_ > 0) { // 進捗グラフ
			float progress = std::min(progress_, 1.0f);
			rect.setRight(rect.left() + rect.width() * progress);
			pr.fillRect(rect, Qt::green);
		} else if (progress_ < 0) { // 実行中（縞模様）
			pr.setBrushOrigin(QPoint(animation_ % 16, 0));
			pr.fillRect(rect, pattern_brush_);
		}
	}

	if (msg_visible) {
		QLabel::paintEvent(event);
	}
}
