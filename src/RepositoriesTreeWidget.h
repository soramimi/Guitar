#ifndef REPOSITORIESTREEWIDGET_H
#define REPOSITORIESTREEWIDGET_H

#include <QTreeWidget>

class MainWindow;

class RepositoriesTreeWidget : public QTreeWidget {
	Q_OBJECT
private:
	MainWindow *mainwindow();
	QTreeWidgetItem *current_item = nullptr;
protected:
	void dropEvent(QDropEvent *event) override;
	void dragEnterEvent(QDragEnterEvent *event) override;
public:
	explicit RepositoriesTreeWidget(QWidget *parent = nullptr);
signals:
	void dropped();
};

#endif // REPOSITORIESTREEWIDGET_H
