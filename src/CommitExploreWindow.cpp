#include "CommitExploreWindow.h"
#include "ui_CommitExploreWindow.h"
#include "GitObjectManager.h"
#include "FilePreviewWidget.h"
#include "common/misc.h"
#include "MainWindow.h"
#include "main.h"

#include <QFileIconProvider>

static QTreeWidgetItem *newQTreeWidgetItem()
{
	QTreeWidgetItem *item = new QTreeWidgetItem();
	item->setSizeHint(0, QSize(20, 20));
	return item;
}

enum {
	ItemTypeRole = Qt::UserRole,
	ObjectIdRole,
	FilePathRole,
};

struct CommitExploreWindow::Private {
	MainWindow *mainwindow;
	GitObjectCache *objcache;
	Git::CommitItem const *commit;
	QString root_tree_id;
	GitTreeItemList tree_item_list;
	Git::Object content_object;
	ObjectContent content;
    FileDiffWidget::DrawData draw_data;
};

CommitExploreWindow::CommitExploreWindow(MainWindow *parent, GitObjectCache *objcache, const Git::CommitItem *commit)
	: QDialog(parent)
	, ui(new Ui::CommitExploreWindow)
	, m(new Private)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	flags |= Qt::WindowMaximizeButtonHint;
	setWindowFlags(flags);

	m->mainwindow = parent;

	m->objcache = objcache;
	m->commit = commit;

	ui->widget_fileview->bind(qobject_cast<MainWindow *>(parent));
	ui->widget_fileview->setMaximizeButtonEnabled(false);

	ui->splitter->setSizes({100, 100, 200});

	ui->widget_fileview->setLeftBorderVisible(false);

	ui->widget_fileview->setSingleFile(QByteArray(), QString(), QString());

	// set text
	ui->lineEdit_commit_id->setText(commit->commit_id);
	ui->lineEdit_date->setText(misc::makeDateTimeString(commit->commit_date));
	ui->lineEdit_author->setText(commit->author);

	{
		GitCommit c;
		c.parseCommit(objcache, m->commit->commit_id);
		m->root_tree_id = c.tree_id;
	}

	{
		GitCommitTree tree(objcache);
		tree.parseTree(m->root_tree_id);
	}

	{
		QTreeWidgetItem *rootitem = newQTreeWidgetItem();
		rootitem->setText(0, tr("Commit"));
		rootitem->setData(0, ItemTypeRole, (int)GitTreeItem::TREE);
		rootitem->setData(0, ObjectIdRole, m->root_tree_id);
		ui->treeWidget->addTopLevelItem(rootitem);

		loadTree(m->root_tree_id);

		rootitem->setExpanded(true);
	}
}

CommitExploreWindow::~CommitExploreWindow()
{
	delete m;
	delete ui;
}

void CommitExploreWindow::clearContent()
{
	m->content = ObjectContent();
	ui->widget_fileview->clearDiffView();
}

//static void removeChildren(QTreeWidgetItem *item)
//{
//	int i = item->childCount();
//	while (i > 0) {
//		i--;
//		delete item->takeChild(i);
//	}
//}

void CommitExploreWindow::expandTreeItem_(QTreeWidgetItem *item)
{
	if (item->childCount() == 1) {
		if (item->child(0)->text(0).isEmpty()) {
			delete item->takeChild(0);
		}
	}

	if (item->childCount() == 0) {

		QFileIconProvider icons;

		QString path = item->data(0, FilePathRole).toString();

		QString tree_id = item->data(0, ObjectIdRole).toString();
		loadTree(tree_id);

		for (GitTreeItem const &ti : m->tree_item_list) {
			if (ti.type == GitTreeItem::TREE) {
				QTreeWidgetItem *child = newQTreeWidgetItem();
				child->setIcon(0, icons.icon(QFileIconProvider::Folder));
				child->setText(0, ti.name);
				child->setData(0, ItemTypeRole, (int)ti.type);
				child->setData(0, ObjectIdRole, ti.id);
				child->setData(0, FilePathRole, misc::joinWithSlash(path, ti.name));
				QTreeWidgetItem *placeholder = newQTreeWidgetItem();
				child->addChild(placeholder);
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
	GitCommitTree tree(m->objcache);
	tree.parseTree(tree_id);

	m->tree_item_list = *tree.treelist();

	std::sort(m->tree_item_list.begin(), m->tree_item_list.end(), [](GitTreeItem const &left, GitTreeItem const &right){
		int l = (left.type == GitTreeItem::TREE)  ? 0 : 1;
		int r = (right.type == GitTreeItem::TREE) ? 0 : 1;
		if (l != r) return l < r;
		return left.name.compare(right.name, Qt::CaseInsensitive) < 0;
	});
}

void CommitExploreWindow::doTreeItemChanged_(QTreeWidgetItem *current)
{
	ui->listWidget->clear();

	QString path = current->data(0, FilePathRole).toString();

	QString tree_id = current->data(0, ObjectIdRole).toString();

	loadTree(tree_id);

	QFileIconProvider icons;

	for (GitTreeItem const &ti : m->tree_item_list) {
		QIcon icon = icons.icon(ti.type == GitTreeItem::TREE ? QFileIconProvider::Folder : QFileIconProvider::File);
		QListWidgetItem *p = new QListWidgetItem();
		p->setIcon(icon);
		p->setText(ti.name);
		p->setData(ItemTypeRole, (int)ti.type);
		p->setData(ObjectIdRole, ti.id);
		p->setData(FilePathRole, misc::joinWithSlash(path, ti.name));
		ui->listWidget->addItem(p);
	}
}

void CommitExploreWindow::on_treeWidget_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem * /*previous*/)
{
	clearContent();
	doTreeItemChanged_(current);
}

void CommitExploreWindow::on_listWidget_itemDoubleClicked(QListWidgetItem *item)
{
	Q_ASSERT(item);

	GitTreeItem::Type type = (GitTreeItem::Type)item->data(ItemTypeRole).toInt();
	if (type == GitTreeItem::TREE) {
		QString tree_id = item->data(ObjectIdRole).toString();
		clearContent();
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

void CommitExploreWindow::on_listWidget_currentItemChanged(QListWidgetItem *current, QListWidgetItem * /*previous*/)
{
	if (!current) return;

	GitTreeItem::Type type = (GitTreeItem::Type)current->data(ItemTypeRole).toInt();
	if (type == GitTreeItem::BLOB) {
		QString id = current->data(ObjectIdRole).toString();
		m->content_object = m->objcache->catFile(id);
		QString path = current->data(FilePathRole).toString();
		clearContent();
		ui->widget_fileview->setSingleFile(m->content_object.content, id, path);
	} else {
		clearContent();
	}
}
