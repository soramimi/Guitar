#include "SimpleImageWidget.h"
#include <QDebug>
#include <QMouseEvent>
#include <QPainter>

SimpleImageWidget::SimpleImageWidget(QWidget *parent)
	: QWidget{parent}
{
}

/**
 * @brief 画像を設定
 * @param image
 */
void SimpleImageWidget::setImage(const QImage &image)
{
	image_ = image;
	update();
}

/**
 * @brief SimpleImageWidget::paintEvent
 * @param event
 */
void SimpleImageWidget::paintEvent(QPaintEvent *event)
{
	(void)event;

	// ウィンドウに内接するように画像を描画

	int dw = width();
	int dh = height();
	if (dw < 1 || dh < 1) return;

	int sw = image_.width();
	int sh = image_.height();
	if (sw < 1 || sh < 1) return;

	if (sh * dw < dh * sw) {
		dh = sh * dw / sw;
	} else {
		dw = sw * dh / sh;
	}

	int x = (width() - dw) / 2;
	int y = (height() - dh) / 2;

	QPainter pr(this);
	pr.drawImage(QRect(x, y, dw, dh), image_, QRect(0, 0, sw, sh));
}

void SimpleImageWidget::mouseReleaseEvent(QMouseEvent *event)
{
	// マウス左ボタンが離されたときウィンドウ座標内ならクリックされたことにする
	if (event->button() == Qt::LeftButton) {
		if (rect().contains(event->pos())) {
			emit clicked();
		}
	}
}

