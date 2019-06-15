#include "FilesListWidget.h"

#include <QDebug>
#include <QPainter>
#include <QStyledItemDelegate>

namespace {
class ItemDelegate : public QStyledItemDelegate {
private:
	mutable QIcon icon_chg;
	mutable QIcon icon_add;
	mutable QIcon icon_del;
	mutable QIcon icon_ren;
	mutable QIcon icon_cpy;
public:
	ItemDelegate(QObject *parent)
		: QStyledItemDelegate(parent)
	{
	}
	void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
	{
		QStyleOptionViewItem o = option;
		QStyledItemDelegate::initStyleOption(&o, index);

		int h = o.rect.height();
		int w = 2 + h + painter->fontMetrics().size(0, " Aaa").width() + 2;
#if 1
		QIcon icon;
		QString badge;
		QColor bgcolor(240, 240, 240);
		if (o.text.startsWith("(chg) ")) {
			badge = "Chg";
			o.text = o.text.mid(6);
			bgcolor = QColor(240, 240, 140);
			if (icon_chg.isNull()) {
				icon_chg = QIcon(":/image/chg.svg");
			}
			icon = icon_chg;
		} else if (o.text.startsWith("(add) ")) {
			badge = "Add";
			o.text = o.text.mid(6);
			bgcolor = QColor(180, 240, 180);
			if (icon_add.isNull()) {
				icon_add = QIcon(":/image/add.svg");
			}
			icon = icon_add;
		} else if (o.text.startsWith("(del) ")) {
			badge = "Del";
			o.text = o.text.mid(6);
			bgcolor = QColor(255, 200, 200);
			if (icon_del.isNull()) {
				icon_del = QIcon(":/image/del.svg");
			}
			icon = icon_del;
		} else if (o.text.startsWith("(ren) ")) {
			badge = "Ren";
			o.text = o.text.mid(6);
			bgcolor = QColor(200, 210, 255);
			if (icon_ren.isNull()) {
				icon_ren = QIcon(":/image/ren.svg");
			}
			icon = icon_ren;
		} else if (o.text.startsWith("(cpy) ")) {
			badge = "Cpy";
			o.text = o.text.mid(6);
			bgcolor = QColor(200, 210, 255);
			if (icon_cpy.isNull()) {
				icon_cpy = QIcon(":/image/cpy.svg");
			}
			icon = icon_cpy;
		}
		if (!badge.isEmpty()) {
			int x = o.rect.x();
			int y = o.rect.y();
			QRect r(x, y, w, h);
			QRect r2 = QRect(x + 2, y + 1, h - 2, h - 2).adjusted(2, 2, -2, -2);
			QRect r3 = r.adjusted(1, 1, -2, -2);
			QRect r4 = r.adjusted(r2.width(), 0, 0, 0);
			QTextOption to;
			to.setAlignment(Qt::AlignCenter | Qt::AlignVCenter);
			painter->setPen(Qt::NoPen);
			painter->setBrush(QBrush(bgcolor.darker(130)));
			painter->drawRoundedRect(r3.translated(1, 1), 3, 3);
			painter->setBrush(QBrush(bgcolor));
			painter->drawRoundedRect(r3, 3, 3);
			if (!icon.isNull()) {
				painter->save();
				painter->setOpacity(0.5);
				icon.paint(painter, r2);
				painter->restore();
			}
			painter->setPen(Qt::black);
			painter->drawText(r4, badge, to);
		}
		o.rect.adjust(w, 0, 0, 0);
#endif
		option.widget->style()->drawControl(QStyle::CE_ItemViewItem, &o, painter, option.widget);
	}
};
}

FilesListWidget::FilesListWidget(QWidget *parent)
	: QListWidget(parent)
{
	item_delegate = new ItemDelegate(this);
	setItemDelegate(item_delegate);
}
