#ifndef FILESLISTWIDGET_H
#define FILESLISTWIDGET_H

#include <QListWidget>
#include <QWidget>

class QStyledItemDelegate;

class FilesListWidget : public QListWidget {
	Q_OBJECT
private:
	QStyledItemDelegate *item_delegate = nullptr;
public:
	explicit FilesListWidget(QWidget *parent = nullptr);

signals:

public slots:
};

#endif // FILESLISTWIDGET_H
