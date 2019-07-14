#ifndef STANDARDSTYLE_H
#define STANDARDSTYLE_H

#include "TraditionalWindowsStyleTreeControl.h"

#include <QProxyStyle>


class StandardStyle : public QProxyStyle {
private:
	TraditionalWindowsStyleTreeControl legacy_windows_;
public:
	StandardStyle()
		: QProxyStyle(nullptr)
	{
	}
	void drawPrimitive(PrimitiveElement element, QStyleOption const *option, QPainter *painter, QWidget const *widget = nullptr) const override;
};

#endif // STANDARDSTYLE_H
