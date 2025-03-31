#include "RepositoryTreeWidget.h"
#include <QDropEvent>
#include <QDebug>
#include <QMimeData>
#include <QApplication>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QFileInfo>
#include "IncrementalSearch.h"
#include "MainWindow.h"
#include "ApplicationGlobal.h"
#include "common/joinpath.h"

class RepositoryTreeWidgetItemDelegate : public QStyledItemDelegate {
private:
	static void drawText_default(QPainter *painter, QStyleOptionViewItem const &opt, QRect const &rect, QString const &text)
	{
		int w = painter->fontMetrics().size(Qt::TextSingleLine, text).width();
		QRect r = rect;
		r.setWidth(w);
		if (text.startsWith('[')) {
			int i = text.indexOf(']');
			Q_ASSERT(i >= 0);
			QString s = text.mid(1, i - 1);
			QFont font = painter->font();
			QFont smallfont = font;
			smallfont.setPointSize(font.pointSize() * 4 / 5);
			painter->setFont(smallfont);
			painter->save();
			painter->setOpacity(0.75);
			MigemoFilter::drawText(painter, opt, r, s);
			painter->restore();
			int w = painter->fontMetrics().size(Qt::TextSingleLine, s).width();
			r.translate(w, 0);
			painter->setFont(font);
			painter->setPen(opt.palette.color(QPalette::Text));
			MigemoFilter::drawText(painter, opt, r, text.mid(i + 1));
		} else {
			MigemoFilter::drawText(painter, opt, r, text);
		}
	}
public:
	void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
	{
		QStyleOptionViewItem opt = option;
		initStyleOption(&opt, index);

		RepositoryTreeWidget const *treewidget = qobject_cast<RepositoryTreeWidget const *>(opt.widget);
		Q_ASSERT(treewidget);
		MigemoFilter filter = treewidget->filter();

		QRect iconrect = opt.widget->style()->subElementRect(QStyle::SE_ItemViewItemDecoration, &opt, opt.widget);
		QRect textrect = opt.widget->style()->subElementRect(QStyle::SE_ItemViewItemText, &opt, opt.widget);

		// 背景を描画
		if (!filter.isEmpty()) {
			MigemoFilter::fillFilteredBG(painter, opt.rect);
		}
		opt.widget->style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);

		// アイコンを描画
		opt.icon.paint(painter, iconrect);

		// テキストを描画
		if (filter.isEmpty()) { // フィルターがない場合
			drawText_default(painter, opt, textrect, opt.text);
		} else {
			MigemoFilter::drawText_filted(painter, opt, textrect, filter);
		}
	}
};

struct RepositoryTreeWidget::Private {
	RepositoryTreeWidgetItemDelegate delegate;
	MigemoFilter filter;
};

RepositoryTreeWidget::RepositoryTreeWidget(QWidget *parent)
	: QTreeWidget(parent)
	, m(new Private)
{
	IncrementalSearch::instance()->open();

	setItemDelegate(&m->delegate);
	connect(this, &RepositoryTreeWidget::currentItemChanged, [&](QTreeWidgetItem *current, QTreeWidgetItem *){
		current_item = current;
	});
}

RepositoryTreeWidget::~RepositoryTreeWidget()
{
	delete m;
}

void RepositoryTreeWidget::enableDragAndDrop(bool enabled)
{
	setDragEnabled(enabled);
	setAcceptDrops(enabled);
	setDropIndicatorShown(enabled);
	viewport()->setAcceptDrops(enabled);
}

bool RepositoryTreeWidget::isFiltered() const
{
	return !m->filter.isEmpty();
}

QTreeWidgetItem *RepositoryTreeWidget::newQTreeWidgetItem(QString const &name, Type type, int index)
{
	auto *item = new QTreeWidgetItem;
	item->setSizeHint(0, QSize(20, 20));
	item->setText(0, name);
	switch (type) {
	case Group:
		item->setIcon(0, global->graphics->folder_icon);
		item->setFlags(item->flags() | Qt::ItemIsEditable);
		break;
	case Repository:
		item->setIcon(0, global->graphics->repository_icon);
		item->setFlags(item->flags() & ~Qt::ItemIsDropEnabled);
		break;
	}
	item->setData(0, MainWindow::IndexRole, index);
	return item;
}

QTreeWidgetItem *RepositoryTreeWidget::newQTreeWidgetGroupItem(const QString &name)
{
	return newQTreeWidgetItem(name, Group, MainWindow::GroupItem);
}

QTreeWidgetItem *RepositoryTreeWidget::newQTreeWidgetRepositoryItem(const QString &name, int index)
{
	return newQTreeWidgetItem(name, Repository, index);
}

void RepositoryTreeWidget::setFilter(MigemoFilter const &filter)
{
	m->filter = filter;
}

MigemoFilter const &RepositoryTreeWidget::filter() const
{
	return m->filter;
}

void RepositoryTreeWidget::paintEvent(QPaintEvent *event)
{
	QTreeWidget::paintEvent(event);
}

void RepositoryTreeWidget::setRepositoryListStyle(RepositoryListStyle style)
{
	current_repository_list_style_ = style;
}

static QDateTime repositoryLastModifiedTime(QString const &path)
{
#if 0
	QFileInfo info(path);
	return info.lastModified();
#else
	GitRunner g = global->mainwindow->git(path, {}, {}, false);
	return g.repositoryLastModifiedTime();
#endif
}

void RepositoryTreeWidget::updateList(RepositoryListStyle style, QList<RepositoryInfo> const &repos, QString const &filtertext, int select_row)
{
	RepositoryTreeWidget *tree = this;
	tree->clear();

	// リポジトリリストを更新（標準）
	auto UpdateRepositoryListStandard = [&](QString const &filtertext){

		MigemoFilter filter;
		filter.makeFilter(filtertext);

		enableDragAndDrop(filtertext.isEmpty()); // フィルタが空の場合はドラッグ＆ドロップを有効にする
		tree->setFilter(filter);

		QTreeWidgetItem *select_item = nullptr;


		std::map<QString, QTreeWidgetItem *> parentmap;

		for (int i = 0; i < repos.size(); i++) {
			RepositoryInfo const &repo = repos.at(i);

			if (!filter.match(repo.name)) continue;

			QTreeWidgetItem *parent = nullptr;
			{
				QString groupname = repo.group;
				if (groupname.startsWith('/')) { // 先頭のスラッシュを削除
					groupname = groupname.mid(1);
				}
				if (groupname == "") { // グループ名が空の場合はデフォルトに設定
					groupname = "Default";
				}
				auto it = parentmap.find(groupname); // 親アイテムを取得
				if (it != parentmap.end()) {
					parent = it->second;
				} else { // 親アイテムが見つからない場合は新規作成
					QStringList list = groupname.split('/', _SkipEmptyParts);
					if (list.isEmpty()) {
						list.push_back("Default");
					}
					QString groupPath;
					for (QString const &name : list) {
						if (name.isEmpty()) continue;
						QString groupPathWithCurrent = groupPath + name; // グループパスに現在のグループ名を追加
						auto it = parentmap.find(groupPathWithCurrent);
						if (it != parentmap.end()) {
							parent = it->second;
						} else {
							QTreeWidgetItem *newitem = newQTreeWidgetGroupItem(name);
							if (!parent) {
								tree->addTopLevelItem(newitem); // トップレベルに追加
							} else {
								parent->addChild(newitem); // 親の子
							}
							parent = newitem;
							parentmap[groupPathWithCurrent] = newitem; // parentmapに登録
							newitem->setExpanded(true); // 展開
						}
						groupPath = groupPathWithCurrent + '/';
					}
					Q_ASSERT(parent);
				}
			}
			parent->setData(0, MainWindow::FilePathRole, "");

			auto *child = newQTreeWidgetRepositoryItem(repo.name, i);
			parent->addChild(child);
			parent->setExpanded(true);

			if (i == select_row) {
				select_item = child;
			}
		}
		if (select_item) {
			setCurrentItem(select_item);
		}
	};

	// リポジトリリストを更新（最終更新日時順）
	auto UpdateRepositoryListSortRecent = [&](){

		enableDragAndDrop(false); // ドラッグ＆ドロップを無効にする
		tree->setFilter({});

		struct Item {
			int index;
			RepositoryInfo const *data;
			QDateTime lastModified;
		};
		std::vector<Item> items;
		{
			GlobalSetOverrideWaitCursor();
			for (int i = 0; i < repos.size(); i++) {
				{
					mainwindow()->showProgress(tr("Querying last modified time of %1/%2").arg(i + 1).arg(repos.size()));
					mainwindow()->setProgress((float)i / repos.size());
					QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
				}
				RepositoryInfo const &item = repos.at(i);
				QDateTime lastmodified = repositoryLastModifiedTime(item.local_dir);
				items.push_back({i, &item, lastmodified});
			}
			std::sort(items.begin(), items.end(), [](Item const &a, Item const &b){
				return a.lastModified > b.lastModified;
			});
			mainwindow()->hideProgress();
			GlobalRestoreOverrideCursor();
		}

		for (Item const &item : items) {
			QString s = misc::makeDateTimeString(item.lastModified);
			s = QString("[%1] %2").arg(s).arg(item.data->name);
			auto *child = newQTreeWidgetRepositoryItem(s, item.index);
			tree->addTopLevelItem(child);
		}
	};

	// リポジトリリストを更新
	switch (style) {
	case RepositoryTreeWidget::RepositoryListStyle::_Keep:
		// nop
		break;
	case RepositoryTreeWidget::RepositoryListStyle::Standard:
		UpdateRepositoryListStandard(filtertext);
		break;
	case RepositoryTreeWidget::RepositoryListStyle::SortRecent:
		UpdateRepositoryListSortRecent();
		break;
	default:
		Q_ASSERT(false);
		break;
	}
}

RepositoryTreeWidget::RepositoryListStyle RepositoryTreeWidget::currentRepositoryListStyle() const
{
	return current_repository_list_style_;
}

MainWindow *RepositoryTreeWidget::mainwindow()
{
	return qobject_cast<MainWindow *>(window());
}

void RepositoryTreeWidget::dragEnterEvent(QDragEnterEvent *event)
{
	if (QApplication::modalWindow()) return;

	if (event->mimeData()->hasUrls()) {
		event->acceptProposedAction();
		event->accept();
		return;
	}
	QTreeWidget::dragEnterEvent(event);
}

void RepositoryTreeWidget::dropEvent(QDropEvent *event)
{
	if (QApplication::modalWindow()) return;

	if (0) { // debug
		QMimeData const *mimedata = event->mimeData();
		QByteArray encoded = mimedata->data("application/x-qabstractitemmodeldatalist");
		QDataStream stream(&encoded, QIODevice::ReadOnly);
		while (!stream.atEnd()) {
			int row, col;
			QMap<int,  QVariant> roledatamap;
			stream >> row >> col >> roledatamap;
		}
	}

	if (event->mimeData()->hasUrls()) {
		Q_ASSERT(mainwindow());
		QStringList paths;

		QByteArray ba = event->mimeData()->data("text/uri-list");
		if (ba.size() > 4 && memcmp(ba.data(), "h\0t\0t\0p\0", 8) == 0) {
			QString path = QString::fromUtf16((char16_t const *)ba.data(), ba.size() / 2);
			int i = path.indexOf('\n');
			if (i >= 0) {
				path = path.mid(0, i);
			}
			if (!path.isEmpty()) {
				paths.push_back(path);
			}
		} else {
			QList<QUrl> urls = event->mimeData()->urls();
			for (QUrl const &url : urls) {
				QString path = url.url();
				paths.push_back(path);
			}
		}
		for (QString const &path : paths) {
			if (path.startsWith("file://")) {
				int i = 7;
#ifdef Q_OS_WIN
				if (path.utf16()[i] == '/') {
					i++;
				}
#endif
				mainwindow()->addExistingLocalRepository(path.mid(i), false);
			} else if (path.startsWith("https://github.com/")) {
#if 0 // TODO:
				int i = 19;
				int j = path.indexOf('/', i);
				if (j > i) {
					QString username = path.mid(i, j - i);
					QString reponame = path.mid(j + 1);
				}
#endif
			}
		}
	} else {
		QTreeWidgetItem *item = current_item;
		QTreeWidget::dropEvent(event);
		setCurrentItem(item);
		emit dropped();
	}
}

