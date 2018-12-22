#ifndef TRADITIONALWINDOWSSTYLETREECONTROL_H
#define TRADITIONALWINDOWSSTYLETREECONTROL_H

#include <QBrush>
#include <QPixmap>
#include <QStyle>

class QPainter;
class QWidget;

class TraditionalWindowsStyleTreeControl {
private:
	QBrush br_branch;
	QPixmap pm_plus;
	QPixmap pm_minus;
public:
	TraditionalWindowsStyleTreeControl();
	bool drawPrimitive(QStyle::PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget = 0) const;
};

#endif // TRADITIONALWINDOWSSTYLETREECONTROL_H
