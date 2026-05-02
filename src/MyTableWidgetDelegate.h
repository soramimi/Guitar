#ifndef MYTABLEWIDGETDELEGATE_H
#define MYTABLEWIDGETDELEGATE_H

#include <QStyledItemDelegate>


class MyTableWidgetDelegate : public QStyledItemDelegate {
public:
	explicit MyTableWidgetDelegate(QObject *parent = nullptr);
	void paint(QPainter *painter, const QStyleOptionViewItem &option, QModelIndex const &index) const override;
	void paint_bg(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
};

#endif // MYTABLEWIDGETDELEGATE_H
