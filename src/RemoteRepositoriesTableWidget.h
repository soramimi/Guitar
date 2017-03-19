#ifndef REMOTEREPOSITORIESTABLEWIDGET_H
#define REMOTEREPOSITORIESTABLEWIDGET_H

#include <QTableWidget>

class RemoteRepositoriesTableWidget : public QTableWidget {
public:
	RemoteRepositoriesTableWidget(QWidget *parent = 0);

	// QWidget interface
protected:
	void contextMenuEvent(QContextMenuEvent *event);
};

#endif // REMOTEREPOSITORIESTABLEWIDGET_H
