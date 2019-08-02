#include "LogTableWidget.h"
#include "MainWindow.h"
#include "MyTableWidgetDelegate.h"
#include "common/misc.h"
#include <QApplication>
#include <QDebug>
#include <QEvent>
#include <QPainter>
#include <QProxyStyle>
#include <cmath>
#include <map>

struct LogTableWidget::Private {
};

class LogTableWidgetDelegate : public MyTableWidgetDelegate {
private:
	MainWindow *mainwindow() const
	{
		auto *w = dynamic_cast<LogTableWidget *>(QStyledItemDelegate::parent());
		Q_ASSERT(w);
		return w->mainwindow();
	}

	static QColor labelColor(int kind)
	{
		switch (kind) {
		case BasicMainWindow::Label::Head:         return QColor(255, 192, 224); // blue
		case BasicMainWindow::Label::LocalBranch:  return QColor(192, 224, 255); // blue
		case BasicMainWindow::Label::RemoteBranch: return QColor(192, 240, 224); // green
		case BasicMainWindow::Label::Tag:          return QColor(255, 224, 192); // orange
		}
		return QColor(224, 224, 224); // gray
	}

	static QColor hiliteColor(QColor const &color)
	{
		int r = color.red();
		int g = color.green();
		int b = color.blue();
		r = 255 - (255 - r) / 2;
		g = 255 - (255 - g) / 2;
		b = 255 - (255 - b) / 2;
		return QColor(r, g, b);
	}

	static QColor shadowColor(QColor const &color)
	{
		return QColor(color.red() / 2, color.green() / 2, color.blue() / 2);
	}

	void drawSignatureIcon(QPainter *painter, const QStyleOptionViewItem &opt, QModelIndex const &index) const
	{
		if (!opt.widget->isEnabled()) return;

		Git::CommitItem const *commit = mainwindow()->commitItem(index.row());
		if (commit) {
			QIcon icon = mainwindow()->verifiedIcon(commit->signature);
			if (!icon.isNull()) {
				QRect r = opt.rect.adjusted(6, 3, 0, -3);
				int h = r.height();
				int w = h;
				int x = r.x() + r.width() - w;
				int y = r.y();
				icon.paint(painter, x, y, w, h);
			}
		}
	}

	void drawAvatar(QPainter *painter, const QStyleOptionViewItem &opt, QModelIndex const &index) const
	{
		if (!opt.widget->isEnabled()) return;

		int row = index.row();
		QIcon icon = mainwindow()->committerIcon(row);
		if (!icon.isNull()) {
			int h = opt.rect.height();
			int w = h;
			int x = opt.rect.x() + opt.rect.width() - w;
			int y = opt.rect.y();

			painter->save();
			painter->setOpacity(0.5); // 半透明で描画
			icon.paint(painter, x, y, w, h);
			painter->restore();
		}
	}

	void drawLabels(QPainter *painter, const QStyleOptionViewItem &opt, QModelIndex const &index) const
	{
		int row = index.row();
		QList<BasicMainWindow::Label> const *labels = mainwindow()->label(row);
		if (labels) {
			painter->save();
			painter->setRenderHint(QPainter::Antialiasing);
			QFontMetrics fm = painter->fontMetrics();
			const int space = 8;
			int x = opt.rect.x() + opt.rect.width() - 3;
			int x1 = x;
			int y0 = opt.rect.y();
			int y1 = y0 + opt.rect.height() - 1;
			int i = labels->size();
			while (i > 0) {
				i--;
				BasicMainWindow::Label const &label = labels->at(i);
				QString text = misc::abbrevBranchName(label.text + label.info);
				int w = fm.size(0, text).width() + space * 2; // 幅
				int x0 = x1 - w;
				QRect r(x0, y0, x1 - x0, y1 - y0);
				painter->setPen(Qt::NoPen);
				auto DrawRect = [&](int dx, int dy, QColor color){
					painter->setBrush(color);
					painter->drawRoundedRect(r.adjusted(lround(dx + 3), lround(dy + 3), lround(dx - 3), lround(dy - 3)), 3, 3);
				};
				QColor color = labelColor(label.kind);
				QColor hilite = hiliteColor(color);
				QColor shadow = shadowColor(color);
				DrawRect(-1, -1, hilite);
				DrawRect(1, 1, shadow);
				DrawRect(0, 0, color);
				painter->setPen(Qt::black);
				painter->setBrush(Qt::NoBrush);
				QApplication::style()->drawItemText(painter, r.adjusted(space, 0, 0, 0), opt.displayAlignment, opt.palette, true, text);
				x1 = x0;
			}
			painter->restore();
		}
	}

public:
	explicit LogTableWidgetDelegate(QObject *parent = Q_NULLPTR)
		: MyTableWidgetDelegate(parent)
	{
	}
	void paint(QPainter *painter, const QStyleOptionViewItem &option, QModelIndex const &index) const override
	{
		MyTableWidgetDelegate::paint(painter, option, index);

		enum {
			Graph,
			CommitId,
			Date,
			Author,
			Message,
		};

		// signatureの描画
		if (index.column() == CommitId) {
			drawSignatureIcon(painter, option, index);
		}

		// コミット日時
		if (index.column() == Date) {
			Git::CommitItem const *commit = mainwindow()->commitItem(index.row());
			if (commit && commit->strange_date) {
				QColor color(255, 0, 0, 128);
				QRect r = option.rect.adjusted(1, 1, -1, -2);
				misc::drawFrame(painter, r.x(), r.y(), r.width(), r.height(), color, color);
			}
		}

		// avatarの描画
		if (index.column() == Author) {
			drawAvatar(painter, option, index);
		}

		// ラベルの描画
		if (index.column() == Message) {
			drawLabels(painter, option, index);
		}
	}
};

LogTableWidget::LogTableWidget(QWidget *parent)
	: QTableWidget(parent)
	, m(new Private)
{
	setItemDelegate(new LogTableWidgetDelegate(this));
}

LogTableWidget::~LogTableWidget()
{
	delete m;
}

MainWindow *LogTableWidget::mainwindow()
{
	auto *mw = qobject_cast<MainWindow *>(window());
	Q_ASSERT(mw);
	return mw;
}

void drawBranch(QPainterPath *path, double x0, double y0, double x1, double y1, double r, bool bend_early)
{
	const double k = 0.55228475;
	if (x0 == x1) {
		path->moveTo(x0, y0);
		path->lineTo(x1, y1);
	} else {
		double ym = bend_early ? (y0 + r) : (y1 - r);
		double h = fabs(y1 - y0);
		double w = fabs(x1 - x0);
		if (r > h / 2) r = h / 2;
		if (r > w / 2) r = w / 2;
		double s = r;
		if (x0 > x1) r = -r;
		if (y0 > y1) s = -s;

		if (0) {
			path->moveTo(x0, y0);
			path->lineTo(x0, ym - s);
			path->cubicTo(x0, ym - s + s * k, x0 + r - r * k, ym, x0 + r, ym);
			path->lineTo(x1 - r, ym);
			path->cubicTo(x1 - r + r * k, ym, x1, ym + s - s * k, x1, ym + s);
			path->lineTo(x1, y1);
		} else {
			if (bend_early) {
				path->moveTo(x0, y0);
				path->cubicTo(x0, ym, x1, ym, x1, ym + ym - y0);
				path->lineTo(x1, y1);
			} else {
				path->moveTo(x0, y0);
				path->lineTo(x0, ym + ym - y1);
				path->cubicTo(x0, ym, x1, ym, x1, y1);
			}
		}
	}
}

void LogTableWidget::paintEvent(QPaintEvent *e)
{
	if (rowCount() < 1) return;

	QTableWidget::paintEvent(e);

	QPainter pr(viewport());
	pr.setRenderHint(QPainter::Antialiasing);
	pr.setBrush(QBrush(QColor(255, 255, 255)));

	Git::CommitItemList const *list = &mainwindow()->getLogs();

	int indent_span = 16;

	int line_width = 2;
	int thick_line_width = 4;

	auto ItemRect = [&](int row){
		QRect r;
		QTableWidgetItem *p = item(row, 0);
		if (p) {
			r = visualItemRect(p);
		}
		return r;
	};

	auto IsAncestor = [&](Git::CommitItem const &item){
		return mainwindow()->isAncestorCommit(item.commit_id);
	};

	auto ItemPoint = [&](int depth, QRect const &rect){
		int h = rect.height();
		double n = h / 2.0;
		double x = floor(rect.x() + n + depth * indent_span);
		double y = floor(rect.y() + n);
		return QPointF(x, y);
	};

	auto SetPen = [&](QPainter *pr, int level, bool thick){
		QColor c = mainwindow()->color(level + 1);
		Qt::PenStyle s = Qt::SolidLine;
		pr->setPen(QPen(c, thick ? thick_line_width : line_width, s));
	};

	auto DrawLine = [&](size_t index, int itemrow){
		QRect rc1;
		if (index < list->size()) {
			Git::CommitItem const &item1 = list->at(index);
			rc1 = ItemRect(itemrow);
			QPointF pt1 = ItemPoint(item1.marker_depth, rc1);
			double halfheight = rc1.height() / 2.0;
			for (TreeLine const &line : item1.parent_lines) {
				if (line.depth >= 0) {
					QPainterPath *path = nullptr;
					Git::CommitItem const &item2 = list->at(line.index);
					QRect rc2 = ItemRect(line.index);
					if (index + 1 == (size_t)line.index || line.depth == item1.marker_depth || line.depth == item2.marker_depth) {
						QPointF pt2 = ItemPoint(line.depth, rc2);
						if (pt2.y() > 0) {
							path = new QPainterPath();
							drawBranch(path, pt1.x(), pt1.y(), pt2.x(), pt2.y(), halfheight, line.bend_early);
						}
					} else {
						QPointF pt3 = ItemPoint(item2.marker_depth, rc2);
						if (pt3.y() > 0) {
							path = new QPainterPath();
							QRect rc3 = ItemRect(itemrow + 1);
							QPointF pt2 = ItemPoint(line.depth, rc3);
							drawBranch(path, pt1.x(), pt1.y(), pt2.x(), pt2.y(), halfheight, true);
							drawBranch(path, pt2.x(), pt2.y(), pt3.x(), pt3.y(), halfheight, false);
						}
					}
					if (path) {
						SetPen(&pr, line.color_number, IsAncestor(item1));
						pr.drawPath(*path);
						delete path;
					}
				}
			}
		}
		return rc1.y();
	};

	auto DrawMark = [&](size_t index, int row){
		double x, y;
		y = 0;
		if (index < list->size()) {
			Git::CommitItem const &item = list->at(index);
			QRect rc = ItemRect(row);
			QPointF pt = ItemPoint(item.marker_depth, rc);
			double r = 4;
			x = pt.x() - r;
			y = pt.y() - r;
			if (item.resolved) {
				// ◯
				SetPen(&pr, item.marker_depth, IsAncestor(item));
				pr.drawEllipse((int)x, (int)y, int(r * 2), int(r * 2));
			} else {
				// ▽
				SetPen(&pr, item.marker_depth, false);
				QPainterPath path;
				path.moveTo(pt.x(), pt.y() + r);
				path.lineTo(pt.x() - r, pt.y() - r);
				path.lineTo(pt.x() + r, pt.y() - r);
				path.lineTo(pt.x(), pt.y() + r);
				pr.drawPath(path);
			}
		}
		return y;
	};

	// draw lines

	pr.setOpacity(0.5);
	pr.setBrush(Qt::NoBrush);

	for (size_t i = 0; i < list->size(); i++) {
		double y = DrawLine(i, i);
		if (y >= height()) break;
	}

	// draw marks

	pr.setOpacity(1);
	pr.setBrush(mainwindow()->color(0));

	for (size_t i = 0; i < list->size(); i++) {
		double y = DrawMark(i, i);
		if (y >= height()) break;
	}
}

void LogTableWidget::resizeEvent(QResizeEvent *e)
{
	mainwindow()->updateAncestorCommitMap();
	QTableWidget::resizeEvent(e);
}

void LogTableWidget::verticalScrollbarValueChanged(int value)
{
	(void)value;
	mainwindow()->updateAncestorCommitMap();
}

