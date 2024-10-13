#ifndef REPOSITORIESTREEWIDGET_H
#define REPOSITORIESTREEWIDGET_H

#include <QTreeWidget>

class MainWindow;

class RepositoriesTreeWidget : public QTreeWidget {
	Q_OBJECT
	friend class RepositoriesTreeWidgetItemDelegate;
private:
	struct Private;
	Private *m;
	MainWindow *mainwindow();
	QTreeWidgetItem *current_item = nullptr;
	QString getFilterText() const;
protected:
	void dropEvent(QDropEvent *event) override;
	void dragEnterEvent(QDragEnterEvent *event) override;
public:
	explicit RepositoriesTreeWidget(QWidget *parent = nullptr);
	~RepositoriesTreeWidget();
	void setFilterText(QString const &filtertext);
signals:
	void dropped();

	// QWidget interface
protected:
	void paintEvent(QPaintEvent *event);
};

#endif // REPOSITORIESTREEWIDGET_H
