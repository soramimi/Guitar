
#include "CommitExploreWindow.h"
#include "ui_CommitExploreWindow.h"
#include "ApplicationGlobal.h"
#include "GitObjectManager.h"
#include "ImageViewWidget.h"
#include "MainWindow.h"
#include "common/misc.h"
#include "platform.h"
#include <QFileIconProvider>
#include <QMenu>
#include <memory>

static QTreeWidgetItem *newQTreeWidgetItem()
{
	auto *item = new QTreeWidgetItem;
	item->setSizeHint(0, QSize(20, 20));
	return item;
}

enum {
	ItemTypeRole = Qt::UserRole,
	ObjectIdRole,
	FilePathRole,
};

struct CommitExploreWindow::Private {
	GitObjectCache *objcache;
	GitCommitItem const *commit;
	QString root_tree_id;
	GitTreeItemList tree_item_list;
	GitObject content_object;
	ObjectContent content;
	TextEditorEnginePtr text_editor_engine;
};

CommitExploreWindow::CommitExploreWindow(QWidget *parent, GitObjectCache *objcache, GitCommitItem const *commit)
	: QDialog(parent)
	, ui(new Ui::CommitExploreWindow)
	, m(new Private)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	flags |= Qt::WindowMaximizeButtonHint;
	setWindowFlags(flags);

	m->objcache = objcache;
	m->commit = commit;

	m->text_editor_engine = std::make_shared<TextEditorEngine>();
	ui->widget_fileview->bind(nullptr, ui->verticalScrollBar, ui->horizontalScrollBar, mainwindow()->themeForTextEditor());
	ui->widget_fileview->setDiffMode(m->text_editor_engine, ui->verticalScrollBar, ui->horizontalScrollBar);

	ui->splitter->setSizes({100, 100, 200});

	// set text
	ui->lineEdit_commit_id->setText(commit->commit_id.toQString());
	ui->lineEdit_date->setText(misc::makeDateTimeString(commit->commit_date));
	ui->lineEdit_author->setText(commit->author);

	GitRunner g = git();
	{
		GitCommit c;
		GitCommit::parseCommit(g, objcache, m->commit->commit_id, &c);
		m->root_tree_id = c.tree_id;
	}

	{
		GitCommitTree tree(objcache);
		tree.parseTree(g, m->root_tree_id);
	}

	{
		QTreeWidgetItem *rootitem = newQTreeWidgetItem();
		rootitem->setText(0, tr("Commit"));
		rootitem->setData(0, ItemTypeRole, (int)GitTreeItem::TREE);
		rootitem->setData(0, ObjectIdRole, m->root_tree_id);
		ui->treeWidget->addTopLevelItem(rootitem);

		loadTree(g, m->root_tree_id);

		rootitem->setExpanded(true);
	}
}

CommitExploreWindow::~CommitExploreWindow()
{
	delete m;
	delete ui;
}

MainWindow *CommitExploreWindow::mainwindow()
{
	return global->mainwindow;
}

GitRunner CommitExploreWindow::git()
{
	return mainwindow()->git();
}

void CommitExploreWindow::clearContent()
{
	m->content = ObjectContent();
}

void CommitExploreWindow::expandTreeItem_(GitRunner g, QTreeWidgetItem *item)
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
		loadTree(g, tree_id);

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
	expandTreeItem_(git(), item);
}

void CommitExploreWindow::loadTree(GitRunner g, QString const &tree_id)
{
	GitCommitTree tree(m->objcache);
	tree.parseTree(g, tree_id);

	m->tree_item_list = *tree.treelist();

	std::sort(m->tree_item_list.begin(), m->tree_item_list.end(), [](GitTreeItem const &left, GitTreeItem const &right){
		int l = (left.type == GitTreeItem::TREE)  ? 0 : 1;
		int r = (right.type == GitTreeItem::TREE) ? 0 : 1;
		if (l != r) return l < r;
		return left.name.compare(right.name, Qt::CaseInsensitive) < 0;
	});
}

void CommitExploreWindow::doTreeItemChanged_(GitRunner g, QTreeWidgetItem *current)
{
	ui->listWidget->clear();

	QString path = current->data(0, FilePathRole).toString();

	QString tree_id = current->data(0, ObjectIdRole).toString();

	loadTree(g, tree_id);

	QFileIconProvider icons;

	for (GitTreeItem const &ti : m->tree_item_list) {
		QIcon icon;
		if (ti.type == GitTreeItem::TREE) {
			icon = icons.icon(QFileIconProvider::Folder);
		} else {
#ifdef Q_OS_WIN
			{
				int i = ti.name.lastIndexOf('.');
				if (i > 0) {
					QString ext = ti.name.mid(i + 1);
					icon = winIconFromExtensionLarge(ext);
				}
			}
#endif
			if (icon.isNull()) {
				icon = icons.icon(QFileIconProvider::File);
			}
		}
		auto *p = new QListWidgetItem;
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
	GitRunner g = git();
	doTreeItemChanged_(g, current);
}

void CommitExploreWindow::on_listWidget_itemDoubleClicked(QListWidgetItem *item)
{
	Q_ASSERT(item);

	GitTreeItem::Type type = (GitTreeItem::Type)item->data(ItemTypeRole).toInt();
	if (type == GitTreeItem::TREE) {
		QString tree_id = item->data(ObjectIdRole).toString();
		clearContent();
		QTreeWidgetItem *parent = ui->treeWidget->currentItem();
		GitRunner g = git();
		expandTreeItem_(g, parent);
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
		QString commit_id = current->data(ObjectIdRole).toString();
		GitRunner g = git();
		m->content_object = m->objcache->catFile(g, GitHash(commit_id));
		QString path = current->data(FilePathRole).toString();
		clearContent();
		std::string mimetype = global->determineFileType(m->content_object.content);
		if (misc::isImage(mimetype)) {
			ui->widget_fileview->setImage(mimetype, m->content_object.content, commit_id, path);
		} else {
			ui->widget_fileview->setText(m->content_object.content, commit_id, path);
		}
	} else {
		clearContent();
	}
}

void CommitExploreWindow::on_verticalScrollBar_valueChanged(int)
{
	ui->widget_fileview->refrectScrollBar();
}

void CommitExploreWindow::on_horizontalScrollBar_valueChanged(int)
{
	ui->widget_fileview->refrectScrollBar();
}

void CommitExploreWindow::on_listWidget_customContextMenuRequested(const QPoint &pos)
{
	(void)pos;
	QListWidgetItem *current = ui->listWidget->currentItem();
	if (!current) return;
	GitTreeItem::Type type = (GitTreeItem::Type)current->data(ItemTypeRole).toInt();
	if (type == GitTreeItem::BLOB) {
		QMenu menu;
		QAction *a_history = menu.addAction("History");
		QAction *a = menu.exec(QCursor::pos() + QPoint(8, -8));
		if (a) {
			if (a == a_history) {
				QString path = current->data(FilePathRole).toString();
				mainwindow()->execFileHistory(path);
				return;
			}
		}
	}
}
