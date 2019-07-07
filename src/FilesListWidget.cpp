#include "FilesListWidget.h"
#include "BasicMainWindow.h"
#include <QDebug>
#include <QPainter>
#include <QStyledItemDelegate>
#include <map>

namespace {
class ItemDelegate : public QStyledItemDelegate {
private:
	struct Badge {
		QString text;
		QColor color;
		QIcon icon;
		Badge() = default;
		Badge(QString const &text, QColor const &color, QIcon const &icon)
			: text(text)
			, color(color)
			, icon(icon)
		{
		}
	};
	std::map<QString, Badge> badge_map;
public:
	ItemDelegate(QObject *parent)
		: QStyledItemDelegate(parent)
	{
		badge_map["(chg) "] = Badge("Chg", QColor(240, 240, 140), QIcon(":/image/chg.svg"));
		badge_map["(add) "] = Badge("Add", QColor(180, 240, 180), QIcon(":/image/add.svg"));
		badge_map["(del) "] = Badge("Del", QColor(255, 200, 200), QIcon(":/image/del.svg"));
		badge_map["(ren) "] = Badge("Ren", QColor(200, 210, 255), QIcon(":/image/ren.svg"));
		badge_map["(cpy) "] = Badge("Cpy", QColor(200, 210, 255), QIcon(":/image/cpy.svg"));
	}
	void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
	{
		QStyleOptionViewItem o = option;
		QStyledItemDelegate::initStyleOption(&o, index);

		QString header = index.data(BasicMainWindow::HeaderRole).toString();

		int x = o.rect.x();
		int y = o.rect.y();
		int h = o.rect.height();
		int w = 2 + h + painter->fontMetrics().size(0, " Aaa").width() + 2;

		// draw badge
		Badge badge;
		if (header == "(unmerged) ") {
			badge = Badge("Unmerged", QColor(255, 80, 160), QIcon());
			w = 4 + painter->fontMetrics().size(0, badge.text).width() + 4;
		}
		{
			QColor color;
			auto it = badge_map.find(header);
			if (it != badge_map.end()) {
				badge = it->second;
				color = badge.color;
			} else {
				color = badge.color.isValid() ? badge.color : QColor(160, 160, 160);
			}
			{
				QRect r(x, y, w, h);
				QRect r_icon = badge.icon.isNull() ? QRect() : QRect(x + 2, y + 1, h - 2, h - 2).adjusted(2, 2, -2, -2);
				QRect r_badge = r.adjusted(1, 1, -2, -2);
				QRect r_text = r.adjusted(r_icon.width(), 0, 0, 0);
				painter->setPen(Qt::NoPen);
				painter->setBrush(QBrush(color.darker(130)));
				painter->drawRoundedRect(r_badge.translated(1, 1), 3, 3);
				painter->setBrush(QBrush(color));
				painter->drawRoundedRect(r_badge, 3, 3);
				if (!badge.icon.isNull()) {
					painter->save();
					painter->setOpacity(0.5);
					badge.icon.paint(painter, r_icon);
					painter->restore();
				}
				painter->setPen(Qt::black);
				QTextOption to;
				to.setAlignment(Qt::AlignCenter | Qt::AlignVCenter);
				if (badge.text.isEmpty()) {
					badge.text = "?";
				}
				painter->drawText(r_text, badge.text, to);
			}
		}
		o.rect.adjust(w, 0, 0, 0);

		// draw text
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
