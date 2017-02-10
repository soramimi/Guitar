#include "CommitExploreWindow.h"
#include "ui_CommitExploreWindow.h"
#include "GitObjectManager.h"
#include "FilePreviewWidget.h"

static QTreeWidgetItem *newQTreeWidgetItem()
{
	QTreeWidgetItem *item = new QTreeWidgetItem();
	item->setSizeHint(0, QSize(20, 20));
	return item;
}

enum {
	ItemTypeRole = Qt::UserRole,
	ObjectIdRole,
};

struct CommitExploreWindow::Private {
	GitObjectCache *objcache;
	QString commit_id;
	QString root_tree_id;
	GitTreeItemList tree_item_list;
    FileDiffWidget::DiffData diff_data;
    FileDiffWidget::DrawData draw_data;
};

CommitExploreWindow::CommitExploreWindow(QWidget *parent, GitObjectCache *objcache, QString commit_id) :
	QDialog(parent),
	ui(new Ui::CommitExploreWindow)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	flags |= Qt::WindowMaximizeButtonHint;
	setWindowFlags(flags);

    pv = new Private();

	pv->objcache = objcache;
	pv->commit_id = commit_id;

    ui->widget_fileview->imbue_(qobject_cast<MainWindow *>(parent), &pv->diff_data, &pv->draw_data);

	ui->splitter->setSizes({100, 100, 200});

	GitCommit commit;
	commit.parseCommit(objcache, commit_id);
	pv->root_tree_id = commit.tree_id;

	GitCommitTree tree(objcache);
	tree.parseTree(pv->root_tree_id);

	QTreeWidgetItem *rootitem = newQTreeWidgetItem();
	rootitem->setText(0, commit_id);
	rootitem->setData(0, ItemTypeRole, (int)GitTreeItem::TREE);
	rootitem->setData(0, ObjectIdRole, pv->root_tree_id);
	ui->treeWidget->addTopLevelItem(rootitem);

	loadTree(pv->root_tree_id);

	rootitem->setExpanded(true);
}

CommitExploreWindow::~CommitExploreWindow()
{
	delete pv;
	delete ui;
}

static void removeChildren(QTreeWidgetItem *item)
{
	int i = item->childCount();
	while (i > 0) {
		i--;
		delete item->takeChild(i);
	}
}

void CommitExploreWindow::expandTreeItem_(QTreeWidgetItem *item)
{
	if (item->childCount() == 1) {
		if (item->child(0)->text(0).isEmpty()) {
			delete item->takeChild(0);
		}
	}

	if (item->childCount() == 0) {

		QString tree_id = item->data(0, ObjectIdRole).toString();
		loadTree(tree_id);

		for (GitTreeItem const &ti : pv->tree_item_list) {
			if (ti.type == GitTreeItem::TREE) {
				QTreeWidgetItem *child = newQTreeWidgetItem();
				child->setText(0, ti.name);
				child->setData(0, ItemTypeRole, (int)ti.type);
				child->setData(0, ObjectIdRole, ti.id);
				QTreeWidgetItem *phitem = newQTreeWidgetItem();
				child->addChild(phitem);
				item->addChild(child);
			}
		}
	}
}

void CommitExploreWindow::on_treeWidget_itemExpanded(QTreeWidgetItem *item)
{
	expandTreeItem_(item);
}

void CommitExploreWindow::loadTree(QString const &tree_id)
{
	GitCommitTree tree(pv->objcache);
	tree.parseTree(tree_id);

	pv->tree_item_list = *tree.treelist();

	std::sort(pv->tree_item_list.begin(), pv->tree_item_list.end(), [](GitTreeItem const &left, GitTreeItem const &right){
		int l = (left.type == GitTreeItem::TREE)  ? 0 : 1;
		int r = (right.type == GitTreeItem::TREE) ? 0 : 1;
		if (l != r) return l < r;
		return left.name.compare(right.name, Qt::CaseInsensitive) < 0;
	});

}

void CommitExploreWindow::doTreeItemChanged_(QTreeWidgetItem *current)
{
	ui->listWidget->clear();

	QString tree_id = current->data(0, ObjectIdRole).toString();

	loadTree(tree_id);

	for (GitTreeItem const &ti : pv->tree_item_list) {
		char const *icon = (ti.type == GitTreeItem::TREE) ? ":/image/folder.png" : ":/image/file.png";
		QListWidgetItem *p = new QListWidgetItem();
		p->setIcon(QIcon(icon));
		p->setText(ti.name);
		p->setData(ItemTypeRole, (int)ti.type);
		p->setData(ObjectIdRole, ti.id);
		ui->listWidget->addItem(p);
	}
}

void CommitExploreWindow::on_treeWidget_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem * /*previous*/)
{
	doTreeItemChanged_(current);
}


void CommitExploreWindow::on_listWidget_itemDoubleClicked(QListWidgetItem *item)
{
	GitTreeItem::Type type = (GitTreeItem::Type)item->data(ItemTypeRole).toInt();
	if (type == GitTreeItem::TREE) {
		QString tree_id = item->data(ObjectIdRole).toString();
		QTreeWidgetItem *parent = ui->treeWidget->currentItem();
		expandTreeItem_(parent);
		parent->setExpanded(true);
		int n = parent->childCount();
		for (int i = 0; i < n; i++) {
			QTreeWidgetItem *child = parent->child(i);
			if (child->data(0, ItemTypeRole).toInt() == GitTreeItem::TREE) {
				QString tid = child->data(0, ObjectIdRole).toString();
				if (tid == tree_id) {
					child->setExpanded(true);
					ui->treeWidget->setCurrentItem(child);
					break;
				}
			}
		}
	}
}
