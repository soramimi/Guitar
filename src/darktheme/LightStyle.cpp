#include "LightStyle.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmapCache>
#include <QTableView>
#include <QWidget>

QColor selectionColor()
{
	return QColor(80, 160, 255);
}

void drawSelectedItemFrame(QPainter *p, QRect rect, bool focus)
{
	QColor color = focus ? selectionColor() : QColor(128, 128, 128);

	int x, y, w, h;
	x = rect.x();
	y = rect.y();
	w = rect.width();
	h = rect.height();

	QString key = QString::asprintf("selection_frame:%02x%02x%02x:%dx%d", color.red(), color.green(), color.blue(), w, h);

	QPixmap pixmap;
	if (!QPixmapCache::find(key, &pixmap)) {
		pixmap = QPixmap(w, h);
		pixmap.fill(Qt::transparent);
		QPainter pr(&pixmap);
		pr.setRenderHint(QPainter::Antialiasing);

		QColor pencolor = color;
		pencolor.setAlpha(128);
		pr.setPen(pencolor);
		pr.setBrush(Qt::NoBrush);

		QPainterPath path;
		path.addRoundedRect(1.5, 1.5, w - 1.5, h - 1.5, 3, 3);

		pr.drawPath(path);

		pr.setClipPath(path);

		int a = color.alpha();
		QColor color0 = color;
		QColor color1 = color;
		color0.setAlpha(128 * a / 255);
		color1.setAlpha(64 * a / 255);
		QLinearGradient gr(QPointF(0, 0), QPointF(0, h));
		gr.setColorAt(0, color0);
		gr.setColorAt(1, color1);
		QBrush br(gr);
		pr.fillRect(0, 0, w, h, br);

		pr.end();
		QPixmapCache::insert(key, pixmap);
	}

	p->drawPixmap(x, y, w, h, pixmap);
}

void LightStyle::drawPrimitive(PrimitiveElement element, QStyleOption const *option, QPainter *painter, QWidget const *widget) const
{
	if (element == QStyle::PE_IndicatorBranch) {
		if (legacy_windows_.drawPrimitive(element, option, painter, widget)) {
			return;
		}
	}
	if (element == PE_PanelItemViewItem) {
		//		p->fillRect(option->rect, colorForItemView(option)); // 選択枠を透過描画させるので背景は描かない
		auto DrawSelectionFrame = [&](QRect const &r){
			bool focus = widget && widget->hasFocus();
			drawSelectedItemFrame(painter, r, focus);
		};
		if (auto const *tableview = qobject_cast<QTableView const *>(widget)) {
			QAbstractItemView::SelectionBehavior selection_behavior = tableview->selectionBehavior();
			if (option->state & State_Selected) {
				painter->save();
				painter->setClipRect(option->rect);
				QRect r = widget->rect();
				if (selection_behavior == QAbstractItemView::SelectionBehavior::SelectRows) {
					r = QRect(r.x(), option->rect.y(), r.width(), option->rect.height());
				} else if (selection_behavior == QAbstractItemView::SelectionBehavior::SelectColumns) {
					r = QRect(option->rect.x(), r.y(), option->rect.y(), r.height());
				}
				DrawSelectionFrame(r);
				painter->restore();
			}
		} else {
			if (option->state & State_Selected) {
				DrawSelectionFrame(option->rect);
			}
		}
		return;
	}
	QProxyStyle::drawPrimitive(element, option, painter, widget);
}

void drawMenuBarBG(QPainter *p, const QStyleOption *option, QWidget const *widget)
{
	int x = option->rect.x();
	int y = widget->y();
	int w = option->rect.width();
	int h = widget->height();
	QLinearGradient gradient;
	gradient.setStart(x, y);
	gradient.setFinalStop(x, y + h / 2.0);
	gradient.setColorAt(0, option->palette.color(QPalette::Light));
	gradient.setColorAt(1, option->palette.color(QPalette::Window));
	p->fillRect(x, y, w, h, gradient);
	p->fillRect(x, y + h - 1, w, 1, option->palette.color(QPalette::Dark));
}

void LightStyle::drawControl(ControlElement element, const QStyleOption *opt, QPainter *p, const QWidget *w) const
{
	bool disabled = !(opt->state & State_Enabled);
	if (element == CE_MenuBarItem) {
		drawMenuBarBG(p, opt, w);
		if (opt->state & State_Selected) {
			drawSelectedItemFrame(p, opt->rect, true);
		}
		if (auto const *o = qstyleoption_cast<QStyleOptionMenuItem const *>(opt)) {
			QPalette::ColorRole textRole = disabled ? QPalette::Text : QPalette::ButtonText;
			QPixmap pix = o->icon.pixmap(pixelMetric(PM_SmallIconSize, opt, w), QIcon::Normal);
			uint alignment = Qt::AlignCenter | Qt::TextShowMnemonic | Qt::TextDontClip | Qt::TextSingleLine;
			if (!styleHint(SH_UnderlineShortcut, o, w)) {
				alignment |= Qt::TextHideMnemonic;
			}
			if (!pix.isNull()) {
				drawItemPixmap(p, o->rect, alignment, pix);
			} else {
				drawItemText(p, o->rect, alignment, o->palette, o->state & State_Enabled, o->text, textRole);
			}
		}
		return;
	}
	QProxyStyle::drawControl(element, opt, p, w);
}
