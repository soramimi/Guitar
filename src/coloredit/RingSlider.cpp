#include "RingSlider.h"
//#include "misc.h"
#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>
#include <math.h>

void RingSlider::updateGeometry()
{
	handle_size_ = height();

	int x = handle_size_ / 2;
	int w = width() - handle_size_;
	slider_rect_ = QRect(x, 3, w, height() - 6);

	int val = value();
	int max = maximum();
	int handle_x = val * slider_rect_.width() / (max + 1) + slider_rect_.x() - handle_size_ / 2;
	handle_rect_ = QRect(handle_x, 0, handle_size_, handle_size_);

	slider_image_cache_ = QImage();
}

QSize RingSlider::sliderImageSize() const
{
	return slider_rect_.size();
}

void RingSlider::offset(int delta)
{
	setValue(value() + delta);
}

void RingSlider::resizeEvent(QResizeEvent *e)
{
	QWidget::resizeEvent(e);
	updateGeometry();
}

void RingSlider::keyPressEvent(QKeyEvent *e)
{
	int k = e->key();

	switch (k) {
	case Qt::Key_Home:
		setValue(minimum());
		return;
	case Qt::Key_End:
		setValue(maximum());
		return;
	case Qt::Key_Left:
		offset(-singleStep());
		return;
	case Qt::Key_Right:
		offset(singleStep());
		return;
	case Qt::Key_PageDown:
		offset(-pageStep());
		return;
	case Qt::Key_PageUp:
		offset(pageStep());
		return;
	}
}

void RingSlider::paintEvent(QPaintEvent *)
{
	updateGeometry();

	slider_image_cache_ = generateSliderImage();

	QPainter pr(this);
	pr.setRenderHint(QPainter::Antialiasing);

	{ // left rounded cap
		int h = slider_rect_.height();
		QRect r(slider_rect_.x() - h / 2 - 1, slider_rect_.y(), h, h);
		QRectF fr(r);

		QPainterPath path;
		path.addEllipse(fr);
		pr.save();
		pr.setClipPath(path);
		pr.fillRect(fr, Qt::black);
		fr.adjust(1, 1, -1, -1);

		QPainterPath path2;
		path2.addEllipse(fr);
		pr.setClipPath(path2);
		pr.drawImage(fr, slider_image_cache_, QRect(0, 0, 1, slider_image_cache_.height()));
		pr.restore();
	}
	{ // right rounded cap
		int h = slider_rect_.height();
		QRect r(slider_rect_.x() + slider_rect_.width() - h / 2, slider_rect_.y(), h, h);
		QRectF fr(r);

		QPainterPath path;
		path.addEllipse(fr);
		pr.save();
		pr.setClipPath(path);
		pr.fillRect(fr, Qt::black);
		fr.adjust(1, 1, -1, -1);

		QPainterPath path2;
		path2.addEllipse(fr);
		pr.setClipPath(path2);
		pr.drawImage(fr, slider_image_cache_, QRect(slider_image_cache_.width() - 1, 0, 1, slider_image_cache_.height()));
		pr.restore();
	}

	{ // top and bottom border
		int x = slider_rect_.x();
		int y = slider_rect_.y();
		int w = slider_rect_.width();
		int h = slider_rect_.height();
		pr.fillRect(x, y, w, 1, Qt::black);
		pr.fillRect(x, y + h - 1, w, 1, Qt::black);
	}

	pr.drawImage(slider_rect_.adjusted(0, 1, 0, -1), slider_image_cache_, slider_image_cache_.rect());

	// slider handle
	{
		QPainterPath path;
		path.addRect(rect());
		QPainterPath path2;
		path2.addEllipse(handle_rect_.adjusted(4, 4, -4, -4));
		path = path.subtracted(path2);
		pr.setClipPath(path);

		pr.setPen(Qt::NoPen);
		pr.setBrush(Qt::black);
		pr.drawEllipse(handle_rect_);
		pr.setBrush(Qt::white);
		pr.drawEllipse(handle_rect_.adjusted(1, 1, -1, -1));
		pr.setPen(Qt::NoPen);
		pr.setBrush(Qt::black);
		pr.drawEllipse(handle_rect_.adjusted(3, 3, -3, -3));
	}
}

void RingSlider::mousePressEvent(QMouseEvent *e)
{
	int x = e->pos().x();
	if (x < handle_rect_.x()) {
		offset(-pageStep());
		return;
	}
	if (x >= handle_rect_.x() + handle_rect_.width()) {
		offset(pageStep());
		return;
	}

	mouse_press_value_ = value();
	mouse_press_pos_ = e->pos();
}

void RingSlider::mouseMoveEvent(QMouseEvent *e)
{
	if (e->buttons() & Qt::LeftButton) {
		double slider_w = slider_rect_.width();
		double range = maximum() - minimum();
		double x = (mouse_press_value_ - minimum()) * slider_w / range + e->pos().x() - mouse_press_pos_.x();
		int v = int(x * range / slider_w + minimum());
		setValue(v);
		return;
	}
}

void RingSlider::mouseDoubleClickEvent(QMouseEvent *e)
{
	int w = slider_rect_.width();
	if (w > 1) {
		double x = (e->pos().x() - slider_rect_.x()) * (maximum() - minimum()) / (w - 1);
		int v = floor(x + 0.5);
		v = std::clamp(v, minimum(), maximum());
		setValue(v);
	}
}

void RingSlider::wheelEvent(QWheelEvent *e)
{
	int delta = e->delta();
	bool sign = (delta < 0);
	if (sign) delta = -delta;
	delta = (delta + 119) / 120;
	if (sign) delta = -delta;
	offset(delta);
}
