#ifndef COMMITEXPLOREWINDOW_H
#define COMMITEXPLOREWINDOW_H

#include <QDialog>
#include "FileDiffWidget.h"

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
	Private *m;

	void loadTree(QString const &tree_id);
	void doTreeItemChanged_(QTreeWidgetItem *current);
	void expandTreeItem_(QTreeWidgetItem *item);
	MainWindow *mainwindow();
public:
	explicit CommitExploreWindow(QWidget *parent, MainWindow *mainwin, GitObjectCache *objcache, Git::CommitItem const *commit);
	~CommitExploreWindow();

	void clearContent();
private slots:
	void on_treeWidget_itemExpanded(QTreeWidgetItem *item);
	void on_treeWidget_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
	void on_listWidget_itemDoubleClicked(QListWidgetItem *item);
	void on_listWidget_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);
	void on_verticalScrollBar_valueChanged(int);
	void on_horizontalScrollBar_valueChanged(int);
	void on_listWidget_customContextMenuRequested(const QPoint &pos);
};

#endif // COMMITEXPLOREWINDOW_H
