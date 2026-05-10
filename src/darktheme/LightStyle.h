#ifndef LIGHTSTYLE_H
#define LIGHTSTYLE_H

#include "MyCommonStyle.h"
#include "TraditionalWindowsStyleTreeControl.h"
#include <QProxyStyle>

class LightStyle : public MyCommonStyle<QProxyStyle> {
private:
	using Base = MyCommonStyle<QProxyStyle>;
	TraditionalWindowsStyleTreeControl legacy_windows_;
public:
	void drawPrimitive(PrimitiveElement element, QStyleOption const *option, QPainter *painter, QWidget const *widget = nullptr) const override;
	void drawControl(ControlElement element, const QStyleOption *opt, QPainter *p, const QWidget *widget) const override;
	void drawComplexControl(ComplexControl control, const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const override;
};

#endif // LIGHTSTYLE_H
