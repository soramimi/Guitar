#include "MyCommonStyle.h"

#include <QPainter>

bool MyCommonStyleBase::x_styleHint(int *value, QStyle::StyleHint hint, const QStyleOption *option, const QWidget *widget, QStyleHintReturn *returnData) const
{
	switch (hint) {
	case QStyle::SH_UnderlineShortcut:
		*value = 1; // 常にアクセラレータ下線を表示
		return true;
	case QStyle::SH_Menu_MouseTracking:
	case QStyle::SH_MenuBar_MouseTracking:
	case QStyle::SH_ComboBox_ListMouseTracking:
		*value = 1;
		return true;
	}
	return false;
}

bool MyCommonStyleBase::x_pixelMetric(int *value, QStyle::PixelMetric metric, const QStyleOption *option, const QWidget *widget) const
{
	// switch (metric) {
	// default:
	// 	break;
	// }
	return false;
}

void MyCommonStyleBase::x_sizeFromContents(QSize *value, QStyle::ContentsType type, const QStyleOption *option, const QSize &size, const QWidget *widget) const
{
	if (QStyleOptionMenuItem const *o = qstyleoption_cast<QStyleOptionMenuItem const *>(option)) {
		switch (type) {
		case QStyle::CT_MenuBarItem:
			value->rwidth() = o->fontMetrics.size(0, o->text).width() + 4;
			value->rheight() = 24;
			return;
		}
	}
#if 1
	switch (type) {
	case QStyle::CT_PushButton:
		if (const QStyleOptionButton *btn = qstyleoption_cast<const QStyleOptionButton *>(option)) {
			if (!btn->text.isEmpty() && value->width() < 80)
				value->setWidth(80);
			if (!btn->icon.isNull() && btn->iconSize.height() > 16)
				*value -= QSize(0, 2);
		}
		break;
	case QStyle::CT_RadioButton:
	case QStyle::CT_CheckBox:
		*value += QSize(0, 1);
		break;
	case QStyle::CT_ToolButton:
		*value += QSize(2, 2);
		break;
	case QStyle::CT_SpinBox:
		*value += QSize(0, -3);
		break;
	case QStyle::CT_ComboBox:
		*value += QSize(2, 4);
		break;
	case QStyle::CT_LineEdit:
		*value += QSize(0, 4);
		break;
	case QStyle::CT_MenuBarItem:
		*value += QSize(8, 5);
		break;
	case QStyle::CT_SizeGrip:
		*value += QSize(4, 4);
		break;
	case QStyle::CT_MdiControls:
		*value -= QSize(1, 0);
		break;
	}
#endif
}

/**
 * @brief 枠を描く。色は左上と右下で別々に指定できる。
 * @param pr
 * @param x
 * @param y
 * @param w
 * @param h
 * @param color_topleft 左上の色。無効な色の場合は描かない。
 * @param color_bottomright 右下の色。無効な色の場合は描かない。
 */
void MyCommonStyleBase::drawFrame(QPainter *pr, int x, int y, int w, int h, QColor color_topleft, QColor color_bottomright)
{
	if (w < 3 || h < 3) {
		if (w > 0 && h > 0) {
			pr->fillRect(x, y, w, h, color_topleft);
		}
	} else {
		if (color_topleft.isValid()) {
			pr->fillRect(x, y, w - 1, 1, color_topleft);
			pr->fillRect(x, y + 1, 1, h -1, color_topleft);
		}
		if (color_bottomright.isValid()) {
			pr->fillRect(x + w - 1, y, 1, h -1, color_bottomright);
			pr->fillRect(x + 1, y + h - 1, w - 1, 1, color_bottomright);
		}

	}
}

/**
 * @brief 枠を描く。色は左上と右下で別々に指定できる。
 * @param pr
 * @param r
 * @param color_topleft 左上の色。無効な色の場合は描かない。
 * @param color_bottomright 右下の色。無効な色の場合は描かない。
 */
void MyCommonStyleBase::drawFrame(QPainter *pr, QRect const &r, QColor const &color_topleft, QColor const &color_bottomright)
{
	return drawFrame(pr, r.x(), r.y(), r.width(), r.height(), color_topleft, color_bottomright);
}

/**
 * @brief RaisedやSunkenのある枠を描く
 * @param p
 * @param rect
 * @param palette
 * @param state
 */
void MyCommonStyleBase::drawShadedFrame(QPainter *p, const QRect &rect, const QPalette &palette, QStyle::State state)
{
	QColor topleft;
	QColor bottomright;
	if (state & QStyle::State_Raised) {
		topleft = palette.color(QPalette::Light);
		bottomright = palette.color(QPalette::Shadow);
	} else if (state & QStyle::State_Sunken) {
		topleft = palette.color(QPalette::Shadow);
		bottomright = palette.color(QPalette::Light);
	}
	drawFrame(p, rect, topleft, bottomright);
}

