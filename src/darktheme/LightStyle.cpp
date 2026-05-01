#include "LightStyle.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmapCache>
#include <QTableView>
#include <QWidget>

QColor selectionColor()
{
	return QColor(192, 224, 255);
}

void drawSelectedItemFrame(QPainter *p, QRect rect, bool focus)
{
	QColor color = selectionColor();

	int x, y, w, h;
	x = rect.x();
	y = rect.y();
	w = rect.width();
	h = rect.height();

	p->setBrush(color);
	p->setPen(QPen(Qt::black, 1));
	p->drawRect(x, y, w - 1, h - 1);
}

void LightStyle::drawPrimitive(PrimitiveElement element, QStyleOption const *option, QPainter *painter, QWidget const *widget) const
{
	if (element == QStyle::PE_IndicatorBranch) {
		if (legacy_windows_.drawPrimitive(element, option, painter, widget)) {
			return;
		}
	}
	if (element == PE_PanelItemViewItem) {
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
	bool enabled = (opt->state & State_Enabled);
	bool disabled = !enabled;
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
	if (element == CE_ItemViewItem) {
		if (auto const *o = qstyleoption_cast<QStyleOptionViewItem const *>(opt)) {
			p->save();
			p->setClipRect(o->rect);

			QRect checkRect = subElementRect(SE_ItemViewItemCheckIndicator, o, w);
			QRect iconRect = subElementRect(SE_ItemViewItemDecoration, o, w);
			QRect textRect = subElementRect(SE_ItemViewItemText, o, w);
			textRect.adjust(2, 0, 0, 0);

			// draw the background
			drawPrimitive(PE_PanelItemViewItem, o, p, w);

			// draw the check mark
			if (o->features & QStyleOptionViewItem::HasCheckIndicator) {
				QStyleOptionViewItem o2(*o);
				o2.rect = checkRect;
				o2.state = o2.state & ~QStyle::State_HasFocus;

				switch (o->checkState) {
				case Qt::Unchecked:
					o2.state |= QStyle::State_Off;
					break;
				case Qt::PartiallyChecked:
					o2.state |= QStyle::State_NoChange;
					break;
				case Qt::Checked:
					o2.state |= QStyle::State_On;
					break;
				}
				drawPrimitive(QStyle::PE_IndicatorItemViewItemCheck, &o2, p, w);
			}

			// draw the icon
			QIcon::Mode mode = QIcon::Normal;
			if (!(o->state & QStyle::State_Enabled)) {
				mode = QIcon::Disabled;
			} else if (o->state & QStyle::State_Selected) {
				mode = QIcon::Selected;
			}
			QIcon::State state = o->state & QStyle::State_Open ? QIcon::On : QIcon::Off;
			o->icon.paint(p, iconRect, o->decorationAlignment, mode, state);

			// draw the text
			if (!o->text.isEmpty()) {
				QPalette::ColorGroup cg = o->state & QStyle::State_Enabled ? QPalette::Normal : QPalette::Disabled;
				if (cg == QPalette::Normal && !(o->state & QStyle::State_Active)) {
					cg = QPalette::Inactive;
				}
				if (o->state & QStyle::State_Selected) {
					p->setPen(o->palette.color(cg, QPalette::HighlightedText));
				} else {
					p->setPen(o->palette.color(cg, QPalette::Text));
				}
				if (o->state & QStyle::State_Editing) {
					p->setPen(o->palette.color(cg, QPalette::Text));
					p->drawRect(textRect.adjusted(0, 0, -1, -1));
				}
				drawItemText(p, textRect, o->displayAlignment, o->palette, enabled, o->text, QPalette::Text);
			}

			p->restore();
		}
		return;
	}
	QProxyStyle::drawControl(element, opt, p, w);
}

