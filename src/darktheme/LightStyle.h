#ifndef LIGHTSTYLE_H
#define LIGHTSTYLE_H

#include "TraditionalWindowsStyleTreeControl.h"
#include <QProxyStyle>

class LightStyle : public QProxyStyle {
private:
	TraditionalWindowsStyleTreeControl legacy_windows_;
public:
	LightStyle()
		: QProxyStyle(nullptr)
	{
	}
	void drawPrimitive(PrimitiveElement element, QStyleOption const *option, QPainter *painter, QWidget const *widget = nullptr) const override;

	// QStyle interface
public:
	void drawControl(ControlElement element, const QStyleOption *opt, QPainter *p, const QWidget *widget) const;

	// QStyle interface
public:
	void drawComplexControl(ComplexControl cc, const QStyleOptionComplex *opt, QPainter *p, const QWidget *widget) const;

	// QStyle interface
public:
	int styleHint(StyleHint stylehint, const QStyleOption *opt, const QWidget *widget = nullptr, QStyleHintReturn *returnData = nullptr) const;

	// QStyle interface
public:
	int pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const;
};

#endif // LIGHTSTYLE_H
