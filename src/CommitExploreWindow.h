#ifndef COMMITEXPLOREWINDOW_H
#define COMMITEXPLOREWINDOW_H

#include <QDialog>

namespace Ui {
class CommitExploreWindow;
}

class QTreeWidgetItem;
class QListWidgetItem;
class GitObjectCache;

class CommitExploreWindow : public QDialog
{
	Q_OBJECT
private:
	Ui::CommitExploreWindow *ui;

	struct Private;
	Private *pv;

	void loadTree(const QString &tree_id);
	void doTreeItemChanged_(QTreeWidgetItem *current);
	void expandTreeItem_(QTreeWidgetItem *item);
public:
	explicit CommitExploreWindow(QWidget *parent, GitObjectCache *objcache, QString commit_id);
	~CommitExploreWindow();

private slots:
	void on_treeWidget_itemExpanded(QTreeWidgetItem *item);
	void on_treeWidget_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
	void on_listWidget_itemDoubleClicked(QListWidgetItem *item);
};

#endif // COMMITEXPLOREWINDOW_H
