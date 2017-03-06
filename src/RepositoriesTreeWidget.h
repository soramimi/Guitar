#ifndef REPOSITORIESTREEWIDGET_H
#define REPOSITORIESTREEWIDGET_H

#include <QTreeWidget>

class MainWindow;

class RepositoriesTreeWidget : public QTreeWidget
{
	Q_OBJECT
	MainWindow *mainwindow();
public:
	explicit RepositoriesTreeWidget(QWidget *parent = 0);

signals:
	void dropped();
public slots:

	// QWidget interface
protected:
	void dropEvent(QDropEvent *event);

	// QWidget interface
protected:
	void dragEnterEvent(QDragEnterEvent *event);
};

#endif // REPOSITORIESTREEWIDGET_H
