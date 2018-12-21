#include "MyTableWidgetDelegate.h"

#include <QPainter>
#include <QTableWidget>
#include <QApplication>


MyTableWidgetDelegate::MyTableWidgetDelegate(QObject *parent)
	: QStyledItemDelegate(parent)
{
}

void MyTableWidgetDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
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
		qApp->style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &o, painter, nullptr);
		painter->restore();
	}

	opt.state &= ~QStyle::State_Selected; // 行の選択枠は描画しない
#endif
	opt.state &= ~QStyle::State_HasFocus; // セルのフォーカス枠は描画しない

	QStyledItemDelegate::paint(painter, opt, index); // デフォルトの描画
}
