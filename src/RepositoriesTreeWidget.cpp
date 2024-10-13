#include "RepositoriesTreeWidget.h"
#include <QDropEvent>
#include <QDebug>
#include <QMimeData>
#include <QApplication>
#include <QStyledItemDelegate>
#include <QPainter>
#include "MainWindow.h"

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

class RepositoriesTreeWidgetItemDelegate : public QStyledItemDelegate {
public:
	void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
	{
#if 1
		QStyleOptionViewItem opt = option;
		initStyleOption(&opt, index);

		RepositoriesTreeWidget const *treewidget = qobject_cast<RepositoriesTreeWidget const *>(opt.widget);
		Q_ASSERT(treewidget);
		QString filtertext = treewidget->getFilterText();

		QRect iconrect = opt.widget->style()->subElementRect(QStyle::SE_ItemViewItemDecoration, &opt, opt.widget);
		QRect textrect = opt.widget->style()->subElementRect(QStyle::SE_ItemViewItemText, &opt, opt.widget);

		// 背景を描画
		if (!filtertext.isEmpty()) {
			painter->fillRect(opt.rect, QColor(128, 128, 128, 64));
		}
		opt.widget->style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);

		// アイコンを描画
		opt.icon.paint(painter, iconrect);

		// テキストを描画
		std::vector<std::tuple<QString, bool>> list;
		if (filtertext.isEmpty()) { // フィルターがない場合
			list.push_back(std::make_tuple(opt.text, false));
		} else {
			// フィルターがある場合
			const int filtersize = filtertext.size();
			int left = 0;
			int right = 0;
			while (right < opt.text.size()) { // テキストをフィルターで分割
				if (u16ncmp((ushort const *)opt.text.utf16() + right, (ushort const *)filtertext.utf16(), filtersize) == 0) {
					if (left < right) {
						list.push_back(std::make_tuple(opt.text.mid(left, right - left), false));
					}
					list.push_back(std::make_tuple(opt.text.mid(right, filtersize), true));
					left = right = right + filtersize;
				} else {
					right++;
				}
			}
			if (left < right) { // フィルターで分割できなかった残り
				list.push_back(std::make_tuple(opt.text.mid(left, right - left), false));
			}
		}
		int x = textrect.x();
		for (auto [s, f] : list) {
			int w = painter->fontMetrics().size(Qt::TextSingleLine, s).width();
			QRect r = textrect;
			r.setLeft(x);
			r.setWidth(w);
			if (f) { // フィルターの部分をハイライト
				QColor color = opt.palette.color(QPalette::Highlight);
				color.setAlpha(128);
				painter->fillRect(r, color);
			}
#ifndef Q_OS_WIN
			if (opt.state & QStyle::State_Selected) { // 選択されている場合
				painter->setPen(opt.palette.color(QPalette::HighlightedText));
			} else {
				painter->setPen(opt.palette.color(QPalette::Text));
			}
#endif
			painter->drawText(r, opt.displayAlignment, s); // テキストを描画
			x += w;
		}
#else
		QStyledItemDelegate::paint(painter, option, index);
#endif
	}
};

struct RepositoriesTreeWidget::Private {
	RepositoriesTreeWidgetItemDelegate delegate;
	QString filter_text;
};

RepositoriesTreeWidget::RepositoriesTreeWidget(QWidget *parent)
	: QTreeWidget(parent)
	, m(new Private)
{
	setItemDelegate(&m->delegate);
	connect(this, &RepositoriesTreeWidget::currentItemChanged, [&](QTreeWidgetItem *current, QTreeWidgetItem *){
		current_item = current;
	});
}

RepositoriesTreeWidget::~RepositoriesTreeWidget()
{
	delete m;
}

void RepositoriesTreeWidget::setFilterText(const QString &filtertext)
{
	m->filter_text = filtertext;
	update();
}

QString RepositoriesTreeWidget::getFilterText() const
{
	return m->filter_text;
}

void RepositoriesTreeWidget::paintEvent(QPaintEvent *event)
{
	QTreeWidget::paintEvent(event);
}

MainWindow *RepositoriesTreeWidget::mainwindow()
{
	return qobject_cast<MainWindow *>(window());
}

void RepositoriesTreeWidget::dragEnterEvent(QDragEnterEvent *event)
{
	if (QApplication::modalWindow()) return;

	if (event->mimeData()->hasUrls()) {
		event->acceptProposedAction();
		event->accept();
		return;
	}
	QTreeWidget::dragEnterEvent(event);
}

void RepositoriesTreeWidget::dropEvent(QDropEvent *event)
{
	if (QApplication::modalWindow()) return;

	if (0) {
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
				int i = 19;
				int j = path.indexOf('/', i);
				if (j > i) {
					QString username = path.mid(i, j - i);
					QString reponame = path.mid(j + 1);
				}
			}
		}
	} else {
		QTreeWidgetItem *item = current_item;
		QTreeWidget::dropEvent(event);
		setCurrentItem(item);
		emit dropped();
	}
}

