#ifndef LEGACYWINDOWSSTYLETREECONTROL_H
#define LEGACYWINDOWSSTYLETREECONTROL_H

#include <QBrush>
#include <QPixmap>
#include <QStyle>

class QPainter;
class QWidget;

class LegacyWindowsStyleTreeControl {
private:
	QBrush br_branch;
	QPixmap pm_plus;
	QPixmap pm_minus;
public:
	LegacyWindowsStyleTreeControl();
	bool drawPrimitive(QStyle::PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget = 0) const;
};

#endif // LEGACYWINDOWSSTYLETREECONTROL_H
