#include "MyTableWidgetDelegate.h"

#include <QPainter>
#include <QTableWidget>
#include <QApplication>


MyTableWidgetDelegate::MyTableWidgetDelegate(QObject *parent)
	: QStyledItemDelegate(parent)
{
}

namespace {
void drawFocusFrame(QPainter *p, QRect const &rect, int margin)
{
	p->save();
	p->setRenderHint(QPainter::Antialiasing);
	QColor color(80, 160, 255);
	p->setPen(color);
	p->setBrush(Qt::NoBrush);
	double m = margin + 0.5;
	p->drawRoundedRect(((QRectF)rect).adjusted(m, m, -m, -m), 3, 3);
	p->restore();
}
}

void MyTableWidgetDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, QModelIndex const &index) const
{
	QStyleOptionViewItem opt = option;

#ifdef Q_OS_WIN
	// 選択枠を描画
	if (option.showDecorationSelected) {
		QTableWidget const *tablewidget = qobject_cast<QTableWidget const *>(option.widget);
		Q_ASSERT(tablewidget);
		int w = tablewidget->viewport()->rect().width();
		painter->save();
		QStyleOptionViewItem o = option;
		painter->setClipRect(o.rect);
		o.rect = QRect(1, o.rect.y(), w - 2, o.rect.height());
		QWidget *wid = tablewidget->viewport(); // ダークスタイル時、PE_PanelItemViewItem で選択枠を描画させないため、QTableWidget*ではなくviewportを渡す。
		qApp->style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &o, painter, wid);
		painter->restore();

#if 0
		// フォーカス枠
		if ((opt.state & QStyle::State_Selected) && tablewidget->hasFocus()) {
			if (tablewidget->selectionMode() == QAbstractItemView::SingleSelection && tablewidget->selectionBehavior() == QAbstractItemView::SelectRows) {
				if (auto const *viewport = tablewidget->viewport()) {
					if (auto const *o = qstyleoption_cast<QStyleOptionViewItem const *>(&option)) {
						QRect r(0, option.rect.y(), viewport->width(), option.rect.height());
						if (o->index.column() == tablewidget->model()->columnCount() - 1) {
							r.setWidth(o->rect.x() + o->rect.width());
						}
						painter->save();
						painter->setClipRect(o->rect);
						drawFocusFrame(painter, r, 0);
						painter->restore();
					}
				}
			}
		}
#endif
	}

	opt.state &= ~QStyle::State_Selected; // 行の選択枠は描画しない
#endif
	opt.state &= ~QStyle::State_HasFocus; // セルのフォーカス枠は描画しない

	QStyledItemDelegate::paint(painter, opt, index); // デフォルトの描画
}
