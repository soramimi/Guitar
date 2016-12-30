#ifndef REPOSITORIESTREEWIDGET_H
#define REPOSITORIESTREEWIDGET_H

#include <QTreeWidget>

class RepositoriesTreeWidget : public QTreeWidget
{
	Q_OBJECT
public:
	explicit RepositoriesTreeWidget(QWidget *parent = 0);

signals:
	void dropped();
public slots:

	// QWidget interface
protected:
	void dropEvent(QDropEvent *event);
};

#endif // REPOSITORIESTREEWIDGET_H
