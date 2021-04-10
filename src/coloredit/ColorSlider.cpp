#include "ColorSlider.h"
#include <functional>

ColorSlider::ColorSlider(QWidget *parent)
	: RingSlider(parent)
{
	color_ = Qt::white;
	setVisualType(HSV_H);
}

ColorSlider::VisualType ColorSlider::visualType() const
{
	return color_type_;
}

void ColorSlider::setVisualType(VisualType type)
{
	color_type_ = type;

	int max = (color_type_ == HSV_H) ? 359 : 255;
	setMaximum(max);

	update();
}

void ColorSlider::setColor(QColor const &color)
{
	color_ = color;
	update();
}

QImage ColorSlider::generateSliderImage()
{
	int max = maximum();

	std::function<QColor(int t)> color;

	auto rgb_r = [&](int r){ return QColor(r, color_.green(), color_.blue()); };
	auto rgb_g = [&](int g){ return QColor(color_.red(), g, color_.blue()); };
	auto rgb_b = [&](int b){ return QColor(color_.red(), color_.green(), b); };
	auto hsv_h = [&](int h){ return QColor::fromHsv(h, 255, 255); };
	auto hsv_s = [&](int s){ return QColor::fromHsv(color_.hue(), s, color_.value()); };
	auto hsv_v = [&](int v){ return QColor::fromHsv(color_.hue(), color_.saturation(), v); };
	auto unknown = [&](int i){ return QColor(i, i, i); };

	switch (color_type_) {
	case RGB_R: color = rgb_r; break;
	case RGB_G: color = rgb_g; break;
	case RGB_B: color = rgb_b; break;
	case HSV_H: color = hsv_h; break;
	case HSV_S: color = hsv_s; break;
	case HSV_V: color = hsv_v; break;
	default: color = unknown; break;
	}

	int w = sliderImageSize().width();
	QImage img(w, 1, QImage::Format_ARGB32);
	for (int i = 0; i < w; i++) {
		int t = i * (max + 1) / w;
		reinterpret_cast<QRgb *>(img.scanLine(0))[i] = color(t).rgb();
	}

	return img;
}

