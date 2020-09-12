#ifndef COLORSLIDER_H
#define COLORSLIDER_H

#include "RingSlider.h"

class ColorSlider : public RingSlider {
	Q_OBJECT
public:
	enum VisualType {
		RGB_R,
		RGB_G,
		RGB_B,
		HSV_H,
		HSV_S,
		HSV_V,
	};
private:
	QColor color_;
	VisualType color_type_ = HSV_H;
protected:
	QImage generateSliderImage() override;
public:
	explicit ColorSlider(QWidget *parent = nullptr);
	VisualType visualType() const;
	void setVisualType(VisualType type);
	void setColor(const QColor &color);
};

#endif // COLORSLIDER_H
