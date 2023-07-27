#ifndef TRADITIONALWINDOWSSTYLETREECONTROL_H
#define TRADITIONALWINDOWSSTYLETREECONTROL_H

#include <QBrush>
#include <QPixmap>
#include <QStyle>

class QPainter;
class QWidget;

/**
 * @brief Windows95スタイルのツリーコントロールの見た目にする
 */
class TraditionalWindowsStyleTreeControl {
private:
	QBrush br_branch;
	QPixmap pm_plus;
	QPixmap pm_minus;
public:
	TraditionalWindowsStyleTreeControl();
	bool drawPrimitive(QStyle::PrimitiveElement element, QStyleOption const *option, QPainter *painter, QWidget const *widget = nullptr) const;
};

#endif // TRADITIONALWINDOWSSTYLETREECONTROL_H
