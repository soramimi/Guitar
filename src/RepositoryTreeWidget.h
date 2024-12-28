#ifndef REPOSITORYTREEWIDGET_H
#define REPOSITORYTREEWIDGET_H

#include <QTreeWidget>
#include "RepositoryData.h"

class MainWindow;
struct RepositoryData;

class RepositoryTreeWidget : public QTreeWidget {
	Q_OBJECT
	friend class RepositoryTreeWidgetItemDelegate;
public:
	enum class RepositoryListStyle {
		_Keep,
		Standard,
		SortRecent,
	};
private:
	struct Private;
	Private *m;
	MainWindow *mainwindow();
	QTreeWidgetItem *current_item = nullptr;
	QString getFilterText() const;
	RepositoryListStyle current_repository_list_style_ = RepositoryListStyle::Standard;
protected:
	void paintEvent(QPaintEvent *event);
	void dropEvent(QDropEvent *event) override;
	void dragEnterEvent(QDragEnterEvent *event) override;
public:
	static QTreeWidgetItem *newQTreeWidgetItem();
	static QTreeWidgetItem *newQTreeWidgetFolderItem(QString const &name);
public:
	explicit RepositoryTreeWidget(QWidget *parent = nullptr);
	~RepositoryTreeWidget();
	void enableDragAndDrop(bool enabled);
	void setFilterText(QString const &filtertext);
	void setRepositoryListStyle(RepositoryListStyle style);
	RepositoryListStyle currentRepositoryListStyle() const;
	void updateList(RepositoryTreeWidget::RepositoryListStyle style, const QList<RepositoryData> &repos, QString const &filter);
signals:
	void dropped();
};

#endif // REPOSITORYTREEWIDGET_H
