#include "FilesListWidget.h"

#include <QDebug>
#include <QPainter>
#include <QStyledItemDelegate>

namespace {
class ItemDelegate : public QStyledItemDelegate {
public:
	ItemDelegate(QObject *parent)
		: QStyledItemDelegate(parent)
	{
	}
	void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
	{
		QStyleOptionViewItem o = option;
		//		int h = o.rect.height();
		//		int w = h * 2;
		int h = o.rect.height();
		int w = 2 + h + painter->fontMetrics().size(0, "WWW").width() + 2;
		QStyledItemDelegate::initStyleOption(&o, index);
#if 1
		QString badge;
		QColor bgcolor(240, 240, 240);
		if (o.text.startsWith("(chg) ")) {
			badge = "Chg";
			o.text = o.text.mid(6);
			bgcolor = QColor(240, 240, 160);
		} else if (o.text.startsWith("(ren) ")) {
			badge = "Ren";
			o.text = o.text.mid(6);
			bgcolor = QColor(200, 210, 255);
		} else if (o.text.startsWith("(add) ")) {
			badge = "Add";
			o.text = o.text.mid(6);
			bgcolor = QColor(180, 240, 180);
		} else if (o.text.startsWith("(del) ")) {
			badge = "Del";
			o.text = o.text.mid(6);
			bgcolor = QColor(255, 200, 200);
		} else if (o.text.startsWith("(cpy) ")) {
			badge = "Cpy";
			o.text = o.text.mid(6);
//			bgcolor = QColor(255, 200, 200);
		}
		if (!badge.isEmpty()) {
			QRect r(o.rect.x(), o.rect.y(), w, h);
			QRect r2 = r.adjusted(2 + h, 0, 0, 0);
			QTextOption to;
			to.setAlignment(Qt::AlignCenter | Qt::AlignVCenter);
			painter->setPen(Qt::NoPen);
			QRect r3 = r.adjusted(1, 1, -2, -2);
			painter->setBrush(QBrush(bgcolor.darker(130)));
			painter->drawRoundedRect(r3.translated(1, 1), 3, 3);
			painter->setBrush(QBrush(bgcolor));
			painter->drawRoundedRect(r3, 3, 3);
			painter->setPen(Qt::black);
			painter->drawText(r2, badge, to);
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
