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

static int u16ncmp(ushort const *s1, ushort const *s2, int n)
{
	for (int i = 0; i < n; i++) {
		ushort c1 = s1[i];
		ushort c2 = s2[i];
		if (c1 < 128) c1 = toupper(c1);
		if (c2 < 128) c2 = toupper(c2);
		if (c1 != c2) {
			return c1 - c2;
		}
	}
	return 0;
}

static QString normalizeText(QString s)
{
	for (QChar &c : s) {
		if (c >= 'A' && c <= 'Z') { // 大文字を小文字に
			c = QChar(c.unicode() - 'A' + 'a');
		} else if (c >= QChar(0x3041) && c <= QChar(0x3096)) { // ひらがなをカタカナに
			c = QChar(c.unicode() + 0x60);
		} else if (c >= QChar(0xFF61) && c <= QChar(0xFF9F)) { // 半角カナを全角カナに
			c = QChar(c.unicode() - 0xFF61 + 0x30A0);
		} else if (c >= QChar(0xFF21) && c <= QChar(0xFF3A)) { // 全角英大文字を半角英小文字に
			c = QChar(c.unicode() - 0xFF21 + 'a');
		} else if (c >= QChar(0xFF41) && c <= QChar(0xFF5A)) { // 全角英小文字を半角英小文字に
			c = QChar(c.unicode() - 0xFF41 + 'a');
		}
	}
	return s;
}

class RepositoryTreeWidgetItemDelegate : public QStyledItemDelegate {
private:
	void drawText(QPainter *painter, QStyleOptionViewItem const &opt, QRect r, QString const &text) const
	{
#ifndef Q_OS_WIN
		if (opt.state & QStyle::State_Selected) { // 選択されている場合
			painter->setPen(opt.palette.color(QPalette::HighlightedText));
		} else {
			painter->setPen(opt.palette.color(QPalette::Text));
		}
#endif
		painter->drawText(r, opt.displayAlignment, text); // テキストを描画
	}
	void drawText_default(QPainter *painter, QStyleOptionViewItem const &opt, QRect const &rect, QString const &text) const
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
			drawText(painter, opt, r, s);
			painter->restore();
			int w = painter->fontMetrics().size(Qt::TextSingleLine, s).width();
			r.translate(w, 0);
			painter->setFont(font);
			painter->setPen(opt.palette.color(QPalette::Text));
			drawText(painter, opt, r, text.mid(i + 1));
		} else {
			drawText(painter, opt, r, text);
		}
	}
	void drawText_filted(QPainter *painter, QStyleOptionViewItem const &opt, QRect const &rect, RepositoryTreeWidget::Filter const &filter) const
	{
		QString text = opt.text;

		// フィルターに一致する部分をハイライトして描画
		std::vector<std::tuple<QString, bool>> list;

		if (filter.re.get()) { // 正規表現が有効な場合
			text = normalizeText(text);
			QRegularExpressionMatch match = filter.re->match(text);
			int left = 0;
			while (match.hasMatch()) {
				int right = match.capturedStart();
				if (left < right) {
					list.push_back(std::make_tuple(opt.text.mid(left, right - left), false));
				}
				auto start = match.capturedStart();
				left = match.capturedEnd();
				list.push_back(std::make_tuple(opt.text.mid(start, left - start), true));
				match = filter.re->match(text, left);
			}
			if (left < opt.text.size()) {
				list.push_back(std::make_tuple(opt.text.mid(left), false));
			}
		} else {
			// 通常テキストフィルターの場合
			const int filtersize = filter.text.size();
			int left = 0;
			int right = 0;
			while (right < text.size()) { // テキストをフィルターで分割
				if (u16ncmp((ushort const *)text.utf16() + right, (ushort const *)filter.text.utf16(), filtersize) == 0) {
					if (left < right) {
						list.push_back(std::make_tuple(text.mid(left, right - left), false));
					}
					list.push_back(std::make_tuple(text.mid(right, filtersize), true));
					left = right = right + filtersize;
				} else {
					right++;
				}
			}
			if (left < right) { // フィルターで分割できなかった残り
				list.push_back(std::make_tuple(text.mid(left, right - left), false));
			}
		}

		int x = rect.x();
		for (auto [s, f] : list) {
			int w = painter->fontMetrics().size(Qt::TextSingleLine, s).width();
			QRect r = rect;
			r.setLeft(x);
			r.setWidth(w);
			if (f) { // フィルターの部分をハイライト
				QColor color = opt.palette.color(QPalette::Highlight);
				color.setAlpha(128);
				painter->fillRect(r, color);
			}
			drawText(painter, opt, r, s);
			x += w;
		}
	}
public:
	void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
	{
		QStyleOptionViewItem opt = option;
		initStyleOption(&opt, index);

		RepositoryTreeWidget const *treewidget = qobject_cast<RepositoryTreeWidget const *>(opt.widget);
		Q_ASSERT(treewidget);
		RepositoryTreeWidget::Filter filter = treewidget->filter();

		QRect iconrect = opt.widget->style()->subElementRect(QStyle::SE_ItemViewItemDecoration, &opt, opt.widget);
		QRect textrect = opt.widget->style()->subElementRect(QStyle::SE_ItemViewItemText, &opt, opt.widget);

		// 背景を描画
		if (!filter.isEmpty()) {
			painter->fillRect(opt.rect, QColor(128, 128, 128, 64));
		}
		opt.widget->style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);

		// アイコンを描画
		opt.icon.paint(painter, iconrect);

		// テキストを描画
		std::vector<std::tuple<QString, bool>> list;
		if (filter.isEmpty()) { // フィルターがない場合
			drawText_default(painter, opt, textrect, opt.text);
		} else {
			drawText_filted(painter, opt, textrect, filter);
		}
	}
};

struct RepositoryTreeWidget::Private {
	RepositoryTreeWidgetItemDelegate delegate;
	RepositoryTreeWidget::Filter filter;
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

void RepositoryTreeWidget::setFilter(Filter const &filter)
{
	m->filter = filter;
	update();
}

RepositoryTreeWidget::Filter RepositoryTreeWidget::filter() const
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

RepositoryTreeWidget::Filter RepositoryTreeWidget::makeFilter(QString const &filtertext)
{
	RepositoryTreeWidget::Filter filter;
	filter.text = filtertext;
	if (IncrementalSearch::instance()->migemoEnabled()) {
		if (filtertext.size() >= 2) {
			bool re = true;
			int vowel = 0; // 母音の数
			for (QChar c : filtertext) {
				if (c.isLetter()) {
					if (c == 'a' || c == 'i' || c == 'u' || c == 'e' || c == 'o') {
						vowel++;
					}
				} else if (c.isDigit()) {
					// thru
				} else {
					re = false;
					break;
				}
			};
			if (re && vowel >= 1) { // 全体が2文字以上の英数字で、母音が1文字以上含まれる場合
				auto re = IncrementalSearch::instance()->queryMigemo(filtertext.toStdString().c_str());
				if (re) {
					filter.re = std::make_shared<QRegularExpression>(QString::fromStdString(*re), QRegularExpression::CaseInsensitiveOption);
				}
			}
		}
	}
	return filter;
}

bool match(RepositoryData const &repo, RepositoryTreeWidget::Filter const &filter)
{
	if (filter.isEmpty()) return true; // フィルターが空の場合は常にtrue
	if (filter.re.get()) { // 正規表現が有効な場合
		QString text = normalizeText(repo.name);
		if (text.contains(*filter.re)) return true;
		return false;
	}
	// 正規表現が無効な場合
	return repo.name.indexOf(filter.text, 0, Qt::CaseInsensitive) >= 0;
}

void RepositoryTreeWidget::updateList(RepositoryListStyle style, QList<RepositoryData> const &repos, QString const &filtertext, int select_row)
{
	RepositoryTreeWidget *tree = this;
	tree->clear();

	// リポジトリリストを更新（標準）
	auto UpdateRepositoryListStandard = [&](QString const &filtertext){

		Filter filter = makeFilter(filtertext);

		enableDragAndDrop(filtertext.isEmpty()); // フィルタが空の場合はドラッグ＆ドロップを有効にする
		tree->setFilter(filter);

		QTreeWidgetItem *select_item = nullptr;


		std::map<QString, QTreeWidgetItem *> parentmap;

		for (int i = 0; i < repos.size(); i++) {
			RepositoryData const &repo = repos.at(i);

			if (!match(repo, filter)) continue;

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
			RepositoryData const *data;
			QFileInfo info;
		};
		std::vector<Item> items;
		{
			for (int i = 0; i < repos.size(); i++) {
				RepositoryData const &item = repos.at(i);
				QString gitpath = item.local_dir / ".git";
				QFileInfo info(gitpath);
				items.push_back({i, &item, info});
			}
			std::sort(items.begin(), items.end(), [](Item const &a, Item const &b){
				return a.info.lastModified() > b.info.lastModified();
			});
		}

		for (Item const &item : items) {
			QString s = misc::makeDateTimeString(item.info.lastModified());
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

