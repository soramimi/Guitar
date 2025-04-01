#ifndef REPOSITORYTREEWIDGET_H
#define REPOSITORYTREEWIDGET_H

#include <QTreeWidget>
#include "IncrementalSearch.h"
#include "RepositoryInfo.h"

class MainWindow;
struct RepositoryInfo;

class RepositoryTreeWidgetItem : public QTreeWidgetItem {
public:
	enum {
		IndexRole = Qt::UserRole,

	};

};

class RepositoryTreeWidget : public QTreeWidget {
	Q_OBJECT
	friend class RepositoryTreeWidgetItemDelegate;
public:
	enum class RepositoryListStyle {
		_Keep,
		Standard,
		SortRecent,
	};
	enum {
		GroupItem = -1,
	};
private:
	struct Private;
	Private *m;
	MainWindow *mainwindow();
	QTreeWidgetItem *current_item = nullptr;
	const MigemoFilter &filter() const;
	RepositoryListStyle current_repository_list_style_ = RepositoryListStyle::Standard;
protected:
	void paintEvent(QPaintEvent *event) override;
	void dropEvent(QDropEvent *event) override;
	void dragEnterEvent(QDragEnterEvent *event) override;
private:
	enum Type {
		Group,
		Repository,
	};
	static RepositoryTreeWidgetItem *newQTreeWidgetItem(const QString &name, Type kind, int index);
public:
	static RepositoryTreeWidgetItem *newQTreeWidgetGroupItem(QString const &name);
	static RepositoryTreeWidgetItem *newQTreeWidgetRepositoryItem(const QString &name, int index);
public:
	explicit RepositoryTreeWidget(QWidget *parent = nullptr);
	~RepositoryTreeWidget();
	void enableDragAndDrop(bool enabled);
	bool isFiltered() const;
	void setFilter(const MigemoFilter &filter);
	void setRepositoryListStyle(RepositoryListStyle style);
	RepositoryListStyle currentRepositoryListStyle() const;
	void updateList(RepositoryTreeWidget::RepositoryListStyle style, const QList<RepositoryInfo> &repos, const QString &filtertext, int select_row);
	static RepositoryTreeWidgetItem *item_cast(QTreeWidgetItem *item);
	static int repoIndex(QTreeWidgetItem *item);
	static void setRepoIndex(QTreeWidgetItem *item, int index);
	static bool isGroupItem(QTreeWidgetItem *item);
	static QString treeItemName(QTreeWidgetItem *item);
	static QString treeItemGroup(QTreeWidgetItem *item);
	static QString treeItemPath(QTreeWidgetItem *item);
signals:
	void dropped();
};

#endif // REPOSITORYTREEWIDGET_H
