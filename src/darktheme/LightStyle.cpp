#include "LightStyle.h"

#include <QComboBox>
#include <QPainter>
#include <QPainterPath>
#include <QPixmapCache>
#include <QTableView>
#include <QWidget>

QColor selectionColor()
{
	return QColor(192, 224, 255);
}

constexpr static int windowsItemFrame        =  2; // menu item frame width
constexpr static int windowsItemHMargin      =  3; // menu item hor text margin
constexpr static int windowsItemVMargin      =  4; // menu item ver text margin
constexpr static int windowsArrowHMargin     =  6; // arrow horizontal margin
constexpr static int windowsRightBorder      = 15; // right border on windows

namespace {

QColor baseColor(QPalette const &palette)
{
	return palette.color(QPalette::Button);
}

QColor color(QPalette const &palette, int level, int alpha = 255)
{
	QColor c = baseColor(palette).lighter(level * 100 / 255);
	c.setAlpha(alpha);
	return c;
}

void drawFrame(QPainter *pr, int x, int y, int w, int h, QColor color_topleft, QColor color_bottomright)
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

void drawFrame(QPainter *pr, QRect const &r, QColor const &color_topleft, QColor const &color_bottomright)
{
	return drawFrame(pr, r.x(), r.y(), r.width(), r.height(), color_topleft, color_bottomright);
}

void drawSelectedItemFrame(QPainter *p, QRect rect, bool focus)
{
	QPen pen;
	QColor color = selectionColor();
	if (focus) {
		pen = {Qt::black, 1};
	} else {
		QColor gray(224, 224, 224);
		int r = (color.red() + gray.red()) / 2;
		int g = (color.green() + gray.green()) / 2;
		int b = (color.blue() + gray.blue()) / 2;
		color = QColor(r, g, b);
		pen = Qt::NoPen;
	}

	int x, y, w, h;
	x = rect.x();
	y = rect.y();
	w = rect.width();
	h = rect.height();

	p->setBrush(color);
	p->setPen(pen);
	p->drawRect(x, y, w - 1, h - 1);
}

void drawToolButton(QPainter *p, const QStyleOption *option, QPalette const &palette)
{
	p->save();

	bool hover = (option->state & QStyle::State_MouseOver);
	bool pressed = (option->state & QStyle::State_Sunken);
#ifdef Q_OS_MAC
	hover = false;
#endif

	int x = option->rect.x();
	int y = option->rect.y();
	int w = option->rect.width();
	int h = option->rect.height();
	QColor color0, color1;
#ifdef Q_OS_MAC
	if (pressed) {
		color0 = color(72);
		color1 = color(40);
	} else {
		color0 = color(80);
		color1 = color(48);
	}
#else
	if (pressed) {
		color0 = color(palette, 80);
		color1 = color(palette, 48);
	} else if (hover) {
		color0 = color(palette, 96);
		color1 = color(palette, 64);
	} else {
		color0 = color(palette, 80);
		color1 = color(palette, 48);
	}
#endif
	QLinearGradient gr(QPointF(x, y), QPointF(x, y + h));
	gr.setColorAt(0, color0);
	gr.setColorAt(1, color1);
	QBrush br(gr);
	p->fillRect(x, y, w, h, br);

	if (option->state & QStyle::State_Raised) {
		drawFrame(p, option->rect, color(palette, 96), color(palette, 32));
	} else if (option->state & QStyle::State_Sunken) {
		drawFrame(p, option->rect, color(palette, 48), color(palette, 48));
	}

	p->restore();
}

} // namespace

void LightStyle::drawPrimitive(PrimitiveElement element, QStyleOption const *option, QPainter *painter, QWidget const *widget) const
{
	if (element == QStyle::PE_IndicatorBranch) {
		if (legacy_windows_.drawPrimitive(element, option, painter, widget)) {
			return;
		}
	}
	if (element == PE_PanelItemViewRow) {
#if 1
		if (option->state & State_MouseOver) {
			painter->fillRect(option->rect, option->palette.color(QPalette::Window).lighter(95));
		}
#else
		if (const QStyleOptionViewItem *vopt = qstyleoption_cast<const QStyleOptionViewItem *>(option)) {
			QPalette::ColorGroup cg = (widget ? widget->isEnabled() : (vopt->state & QStyle::State_Enabled)) ? QPalette::Normal : QPalette::Disabled;
			if (cg == QPalette::Normal && !(vopt->state & QStyle::State_Active)) {
				cg = QPalette::Inactive;
			}
			if ((vopt->state & QStyle::State_Selected) && vopt->showDecorationSelected) {
				painter->fillRect(vopt->rect, vopt->palette.brush(cg, QPalette::Highlight));
			} else if (vopt->features & QStyleOptionViewItem::Alternate) {
				painter->fillRect(vopt->rect, vopt->palette.brush(cg, QPalette::AlternateBase));
			}
		}
#endif
		return;
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
	Base::drawPrimitive(element, option, painter, widget);
}

static void drawMenuBarBG(QPainter *p, const QStyleOption *option, QWidget const *widget)
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

static void drawGutter(QPainter *p, const QRect &r, QPalette const &palette)
{
	int x = r.x();
	int y = r.y();
	int w = r.width();
	int h = r.height();
	QColor dark = palette.color(QPalette::Dark);
	QColor lite = palette.color(QPalette::Light);
	if (w < h) {
		x += (w - 1) / 2;
		p->fillRect(x, y, 1, h, dark);
		p->fillRect(x + 1, y, 1, h, lite);
	} else if (w > h) {
		y += (h - 1) / 2;
		p->fillRect(x, y, w, 1, dark);
		p->fillRect(x, y + 1, w, 1, lite);
	}
}

void LightStyle::drawControl(ControlElement element, const QStyleOption *opt, QPainter *p, const QWidget *widget) const
{
	bool enabled = (opt->state & State_Enabled);
	bool disabled = !enabled;
	if (element == CE_MenuBarEmptyArea) {
		drawMenuBarBG(p, opt, widget);
		return;
	}
	if (element == CE_MenuBarItem) {
		drawMenuBarBG(p, opt, widget);
		if (opt->state & State_Selected) {
			drawSelectedItemFrame(p, opt->rect, true);
		}
		if (auto const *o = qstyleoption_cast<QStyleOptionMenuItem const *>(opt)) {
			QPalette::ColorRole textRole = disabled ? QPalette::Text : QPalette::ButtonText;
			QPixmap pix = o->icon.pixmap(pixelMetric(PM_SmallIconSize, opt, widget), QIcon::Normal);
			uint alignment = Qt::AlignCenter | Qt::TextShowMnemonic | Qt::TextDontClip | Qt::TextSingleLine;
			if (!styleHint(SH_UnderlineShortcut, o, widget)) {
				alignment |= Qt::TextHideMnemonic;
			}
			if (!pix.isNull()) {
				drawItemPixmap(p, o->rect, alignment, pix);
			} else {
				p->save();
				p->setFont(o->font);
				drawItemText(p, o->rect, alignment, o->palette, o->state & State_Enabled, o->text, textRole);
				p->restore();
			}
		}
		return;
	}
	if (element == CE_ItemViewItem) {
		if (auto const *o = qstyleoption_cast<QStyleOptionViewItem const *>(opt)) {
			p->save();
			p->setClipRect(o->rect);

			QRect checkRect = subElementRect(SE_ItemViewItemCheckIndicator, o, widget);
			QRect iconRect = subElementRect(SE_ItemViewItemDecoration, o, widget);
			QRect textRect = subElementRect(SE_ItemViewItemText, o, widget);
			textRect.adjust(2, 0, 0, 0);

			// draw the background
			drawPrimitive(PE_PanelItemViewItem, o, p, widget);

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
				drawPrimitive(QStyle::PE_IndicatorItemViewItemCheck, &o2, p, widget);
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
				p->save();
				p->setFont(o->font);
				drawItemText(p, textRect, o->displayAlignment, o->palette, enabled, o->text, QPalette::Text);
				p->restore();
			}

			p->restore();
		}
		return;
	}
	if (element == CE_MenuItem) {
#ifdef Q_OS_WIN
		if (opt->state & QStyle::State_Selected) {
			drawSelectedItemFrame(p, opt->rect, true);
		}
		Base::drawControl(element, opt, p, widget);
		return;
#endif
		if (auto const *o = qstyleoption_cast<QStyleOptionMenuItem const *>(opt)) {
#ifdef Q_OS_MAC
			int checkcol = 15;
#else
			// windows always has a check column, regardless whether we have an icon or not
			int checkcol = 25;// / QWindowsXPStylePrivate::devicePixelRatio(widget);
#endif
			const int gutterWidth = 3;// / QWindowsXPStylePrivate::devicePixelRatio(widget);
			QRect rect = opt->rect;

			bool ignoreCheckMark = false;
			if (qobject_cast<const QComboBox *>(widget)) {
				{ // bg
					int x = opt->rect.x();
					int y = opt->rect.y();
					int w = opt->rect.width();
					int h = opt->rect.height();
					p->fillRect(x, y, w, h, opt->palette.color(QPalette::Light));
				}
				ignoreCheckMark = true; //ignore the checkmarks provided by the QComboMenuDelegate
			}

			//draw vertical menu line
			if (opt->direction == Qt::LeftToRight) {
				checkcol += rect.x();
			}

			int x, y, w, h;
			o->rect.getRect(&x, &y, &w, &h);
			int tab = 0;//o->tabWidth;
			bool disabled = !(o->state & QStyle::State_Enabled);
			bool checkable = (o->checkType != QStyleOptionMenuItem::NotCheckable);
			bool checked = checkable && o->checked;
			bool selected = o->state & QStyle::State_Selected;

			if (o->menuItemType == QStyleOptionMenuItem::Separator) {
				QRect r = opt->rect.adjusted(2, 0, -2, 0);
				drawGutter(p, r, widget->palette());
				return;
			}

#ifdef Q_OS_MAC
			//			qDebug() << pixelMetric(PM_SmallIconSize, option, widget);
			QRect vCheckRect = visualRect(option->direction, o->rect, QRect(o->rect.x(), o->rect.y(), 20 - (gutterWidth + o->rect.x()), o->rect.height()));
#else
			QRect vCheckRect = visualRect(opt->direction, o->rect, QRect(o->rect.x(), o->rect.y(), checkcol - (gutterWidth + o->rect.x()), o->rect.height()));
#endif

			if (selected) {
				drawSelectedItemFrame(p, opt->rect, true);
			}

			int xm = windowsItemFrame + checkcol + windowsItemHMargin + (gutterWidth - o->rect.x()) - 1;
			int xpos = o->rect.x() + xm;

			if (checkable && !ignoreCheckMark) {
				const qreal boxMargin = 3.5;
				const qreal boxWidth = checkcol - 2 * boxMargin;
				const int checkColHOffset = windowsItemHMargin + windowsItemFrame - 1;
#ifdef Q_OS_MAC
				QRectF checkRectF(0, option->rect.y(), xpos, option->rect.height());
				QRect checkRect = checkRectF.toRect();
				checkRect.setWidth(checkRect.height());
				checkRect.adjust(windowsItemFrame + (xpos - windowsItemFrame - checkRect.width()) / 2, 0, 0, 0);
#else
				QRectF checkRectF(opt->rect.left() + boxMargin + checkColHOffset, opt->rect.center().y() - boxWidth / 2 + 1, boxWidth, boxWidth);
				QRect checkRect = checkRectF.toRect();
				checkRect.setWidth(checkRect.height()); // avoid .toRect() round error results in non-perfect square
#endif
				checkRect = visualRect(o->direction, o->rect, checkRect);

				QStyleOptionButton box;
				box.QStyleOption::operator=(*opt);
				box.rect = checkRect;
				if (checked) {
					box.state |= State_On;
				}
				drawPrimitive(PE_IndicatorCheckBox, &box, p, widget);
			}


			if (!ignoreCheckMark) {
				if (!o->icon.isNull()) {
					QIcon::Mode mode = disabled ? QIcon::Disabled : QIcon::Normal;
					if (selected && !disabled) {
						mode = QIcon::Active;
					}
					QPixmap pixmap;
					if (checked) {
						pixmap = o->icon.pixmap(pixelMetric(PM_SmallIconSize, opt, widget), mode, QIcon::On);
					} else {
						pixmap = o->icon.pixmap(pixelMetric(PM_SmallIconSize, opt, widget), mode);
					}
					double dpr = pixmap.devicePixelRatio();
					if (dpr > 0) {
						const int pixw = int(pixmap.width() / dpr);
						const int pixh = int(pixmap.height() / dpr);
#ifdef Q_OS_MAC
						QRect pmr(xpos, vCheckRect.y() + (vCheckRect.height() - pixh) / 2, pixw, pixh);
						p->setPen(o->palette.text().color());
						p->drawPixmap(pmr.topLeft(), pixmap);
						xpos += pmr.width() + 4;
#else
						QRect pmr(0, 0, pixw, pixh);
						pmr.moveCenter(vCheckRect.center());
						p->setPen(o->palette.text().color());
						p->drawPixmap(pmr.topLeft(), pixmap);
#endif
					}
				}
			}

			p->setPen(o->palette.buttonText().color());

			const QColor textColor = o->palette.text().color();
			if (disabled) {
				p->setPen(textColor);
			}

			QRect textRect(xpos, y + windowsItemVMargin, w - xm - windowsRightBorder - tab + 1, h - 2 * windowsItemVMargin);
			QRect vTextRect = visualRect(opt->direction, o->rect, textRect);
			QString s = o->text;
			if (!s.isEmpty()) {    // draw text
				p->save();
				int t = s.indexOf(QLatin1Char('\t'));
				int text_flags = Qt::AlignVCenter | Qt::TextShowMnemonic | Qt::TextDontClip | Qt::TextSingleLine;
				if (!styleHint(SH_UnderlineShortcut, o, widget)) {
					text_flags |= Qt::TextHideMnemonic;
				}
				if (t >= 0) {
					p->drawText(vTextRect, text_flags | Qt::AlignRight, s.mid(t + 1));
					s = s.left(t);
				}
				QFont font = o->font;
				if (o->menuItemType == QStyleOptionMenuItem::DefaultItem) {
					font.setBold(true);
				}
				p->setFont(font);
				p->setPen(textColor);
				p->drawText(vTextRect, text_flags | Qt::AlignLeft, s.left(t)); //@
				p->restore();
			}
			if (o->menuItemType == QStyleOptionMenuItem::SubMenu) {// draw sub menu arrow
				int dim = (h - 2 * windowsItemFrame) / 2;
				PrimitiveElement arrow;
				arrow = (opt->direction == Qt::RightToLeft) ? PE_IndicatorArrowLeft : PE_IndicatorArrowRight;
				xpos = x + w - windowsArrowHMargin - windowsItemFrame - dim;
				QRect  vSubMenuRect = visualRect(opt->direction, o->rect, QRect(xpos, y + h / 2 - dim / 2, dim, dim));
				QStyleOptionMenuItem newMI = *o;
				newMI.rect = vSubMenuRect;
				newMI.state = disabled ? State_None : State_Enabled;
				drawPrimitive(arrow, &newMI, p, widget);
			}
		}
		return;
	}
	Base::drawControl(element, opt, p, widget);
}

void LightStyle::drawComplexControl(ComplexControl control, const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const
{
	Base::drawComplexControl(control, option, painter, widget);
}

