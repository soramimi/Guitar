#ifndef FILELISTWIDGET_H
#define FILELISTWIDGET_H

#include <QListWidget>
#include <QWidget>

class QStyledItemDelegate;

class FileListWidget : public QListWidget {
	Q_OBJECT
private:
	QStyledItemDelegate *item_delegate = nullptr;
public:
	explicit FileListWidget(QWidget *parent = nullptr);

signals:

public slots:
};

#endif // FILELISTWIDGET_H
