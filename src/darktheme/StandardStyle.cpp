#include "StandardStyle.h"

void StandardStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
	if (element == QStyle::PE_IndicatorBranch) {
		if (legacy_windows_.drawPrimitive(element, option, painter, widget)) {
			return;
		}
	}
	QProxyStyle::drawPrimitive(element, option, painter, widget);
}
