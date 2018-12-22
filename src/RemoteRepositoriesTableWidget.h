#ifndef REMOTEREPOSITORIESTABLEWIDGET_H
#define REMOTEREPOSITORIESTABLEWIDGET_H

#include <QTableWidget>

class RemoteRepositoriesTableWidget : public QTableWidget {
public:
	RemoteRepositoriesTableWidget(QWidget *parent = nullptr);
protected:
	void contextMenuEvent(QContextMenuEvent *event) override;
};

#endif // REMOTEREPOSITORIESTABLEWIDGET_H
