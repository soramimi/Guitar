#include "ColorButton.h"

#include <QPainter>
#include <QProxyStyle>
#include <QStyleOption>
#include "coloredit/ColorDialog.h"
#include "common/misc.h"

ColorButton::ColorButton(QWidget *parent)
	: QToolButton(parent)
{
	color_ = Qt::red;

	connect(this, &ColorButton::clicked, [&](){
		QColor c = color();
		ColorDialog dlg(this, c);
		if (dlg.exec() == QDialog::Accepted) {
			c = dlg.color();
			setColor(c);
		}
	});
}

/**
 * @brief 色を取得
 * @return
 */
QColor ColorButton::color() const
{
	return color_;
}

/**
 * @brief 色を設定
 * @param color
 */
void ColorButton::setColor(QColor const &color)
{
	color_ = color;
	update();
}

/**
 * @brief 描画
 * @param event
 */
void ColorButton::paintEvent(QPaintEvent *event)
{
	(void)event;
	QPainter pr(this);
	QStyleOptionToolButton o;
	QStyleOptionButton o2;
	initStyleOption(&o);
	o2.rect = o.rect;
	o2.state = o.state;
	int h = o2.rect.height();

	// 色の矩形を描画
	misc::drawFrame(&pr, 0, 1, h - 2, h - 2, Qt::black);
	pr.fillRect(1, 2, h - 4, h - 4, color());

	// ボタン枠を描画
	o2.rect.adjust(h + 2, 0, 0, 0);
	style()->drawControl(QStyle::CE_PushButtonBevel, &o2, &pr, this);

	// ボタンテキストを描画
	o2.rect.adjust(4, 0, 0, 0);
	style()->drawItemText(&pr, o2.rect, Qt::AlignLeft | Qt::AlignVCenter, palette(), isEnabled(), o.text);
}
