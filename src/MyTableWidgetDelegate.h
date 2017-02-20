#ifndef MYTABLEWIDGETDELEGATE_H
#define MYTABLEWIDGETDELEGATE_H

#include <QStyledItemDelegate>


class MyTableWidgetDelegate : public QStyledItemDelegate {

public:
	explicit MyTableWidgetDelegate(QObject *parent = Q_NULLPTR);
	void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
};

#endif // MYTABLEWIDGETDELEGATE_H
