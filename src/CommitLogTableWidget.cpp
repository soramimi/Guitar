#include "CommitLogTableWidget.h"
#include "ApplicationGlobal.h"
#include "BranchLabel.h"
#include "Git.h"
#include "MainWindow.h"
#include "MyTableWidgetDelegate.h"
#include <QHeaderView>
#include <QPainter>
#include <QPainterPath>
#include <cmath>

QString CommitLogTableModel::escapeTooltipText(QString tooltip)
{
	if (!tooltip.isEmpty()) {
		tooltip.replace('\n', ' ');
		tooltip = tooltip.toHtmlEscaped();
		tooltip = "<p style='white-space: pre'>" + tooltip + "</p>";
	}
	return tooltip;
}

CommitLogTableWidget *CommitLogTableModel::tablewidget()
{
	return qobject_cast<CommitLogTableWidget *>(QObject::parent());
}

const CommitRecord &CommitLogTableModel::record(int row) const
{
	return records_[index_[row]];
}

const CommitRecord &CommitLogTableModel::record(QModelIndex const &index) const
{
	return record(index.row());
}

QModelIndex CommitLogTableModel::index(int row, int column, const QModelIndex &parent) const
{
	return createIndex(row, column, (void *)global->mainwindow);
}

QModelIndex CommitLogTableModel::parent(const QModelIndex &child) const
{
	return QModelIndex();
}

int CommitLogTableModel::rowcount() const
{
	return index_.size();
}

int CommitLogTableModel::rowCount(const QModelIndex &parent) const
{
	return rowcount();
}

int CommitLogTableModel::columnCount(const QModelIndex &parent) const
{
	return 5;
}

QVariant CommitLogTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
		switch (section) {
		case 0: return QVariant(tr("Graph"));
		case 1: return QVariant(tr("Commit"));
		case 2: return QVariant(tr("Date"));
		case 3: return QVariant(tr("Author"));
		case 4: return QVariant(tr("Message"));
		}
	}
	return QVariant();
}

QVariant CommitLogTableModel::data(const QModelIndex &index, int role) const
{
	auto row = index.row();
	auto col = index.column();
	if (row >= 0 && row < rowcount()) {
		CommitRecord const &rec = record(row);
		if (role == Qt::DisplayRole) {
			switch (col) {
			case 0: return QVariant();
			case 1: return QVariant(rec.commit_id);
			case 2: return QVariant(rec.datetime);
			case 3: return QVariant(rec.author);
			case 4: return QVariant(rec.message);
			}
		} else if (role == Qt::ToolTipRole) {
			if (col == 4) {
				return QVariant(escapeTooltipText(rec.tooltip));
			}
		} else if (role == Qt::FontRole) {
			if (col == 4) {
				if (rec.bold) {
					QFont font;
					font.setBold(true);
					return QVariant(font);
				}
			}
		}
	}
	return QVariant();
}

void CommitLogTableModel::privateSetFilter(QString const &text)
{
	if (text.isEmpty()) {
		filter_ = {};
		index_.resize(records_.size());
		std::iota(index_.begin(), index_.end(), 0);
	} else {
		filter_.makeFilter(text);
		int n = records_.size();
		index_.clear();
		index_.reserve(n);
		for (size_t i = 0; i < n; i++) {
			if (filter_.match(records_[i].message)) {
				index_.push_back(i);
			}
		}
	}
	filter_text_ = text;
}

bool CommitLogTableModel::setFilter(QString const &text)
{
	if (text == filter_text_) return false;
	beginResetModel();
	privateSetFilter(text);
	endResetModel();
	return true;
}

int CommitLogTableModel::unfilteredIndex(int i) const
{
	return index_[i];
}

void CommitLogTableModel::setRecords(std::vector<CommitRecord> &&records)
{
	beginResetModel();
	records_ = std::move(records);
	privateSetFilter({});
	endResetModel();
}

/**
 * @brief コミットログを描画するためのdelegate
 */
class CommitLogTableWidgetDelegate : public MyTableWidgetDelegate {
private:
	MainWindow *mainwindow() const
	{
		return static_cast<CommitLogTableWidget *>(parent())->mainwindow();
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

	void drawSignatureIcon(QPainter *painter, const QStyleOptionViewItem &opt, GitCommitItem const &commit) const
	{
		if (!opt.widget->isEnabled()) return;

		QIcon icon = mainwindow()->signatureVerificationIcon(commit.commit_id);
		if (!icon.isNull()) {
			QRect r = opt.rect.adjusted(6, 3, 0, -3);
			int h = r.height();
			int w = h;
			int x = r.x() + r.width() - w;
			int y = r.y();
			icon.paint(painter, x, y, w, h);
		}
	}

	void drawAvatar(QPainter *painter, double opacity, const QStyleOptionViewItem &opt, int row) const
	{
		if (!opt.widget->isEnabled()) return;

		int h = opt.rect.height();
		int w = h;
		int x = opt.rect.x() + opt.rect.width() - w;
		int y = opt.rect.y();

		auto icon = mainwindow()->committerIcon(row, {w, h});
		if (!icon.isNull()) {
			painter->save();
			painter->setOpacity(opacity); // 半透明で描画
			painter->drawImage(QRect(x, y, w, h), icon);
			painter->restore();
		}
	}

	void drawLabels(QPainter *painter, const QStyleOptionViewItem &opt, QModelIndex const &index, BranchLabelList const &labels, QString const &current_branch) const
	{
		if (!labels.empty()) {
			painter->save();
			painter->setRenderHint(QPainter::Antialiasing);

			bool show = global->mainwindow->isLabelsVisible(); // ラベル透過モード
			painter->setOpacity(show ? 1.0 : 0.0625);

			const int space = 8;
			int x = opt.rect.x() + opt.rect.width() - 3;
			int x1 = x;
			int y0 = opt.rect.y();
			int y1 = y0 + opt.rect.height() - 1;
			int i = labels.size();
			while (i > 0) {
				i--;

				// ラベル
				BranchLabel const &label = labels.at(i);
				QString text = misc::abbrevBranchName(label.text + label.info);

				// 現在のブランチ名と一致するなら太字
				bool bold = false;
				if (text.startsWith(current_branch)) {
					auto c = text.utf16()[current_branch.size()];
					if (c == 0 || c == ',') {
						bold = true;
					}
				}

				// フォントの設定
				QFont font = painter->font();
				font.setBold(bold);
				painter->setFont(font);

				// ラベルの矩形
				int w = painter->fontMetrics().size(0, text).width() + space * 2; // 幅
				int x0 = x1 - w;
				QRect r(x0, y0, x1 - x0, y1 - y0);

				// ラベル枠の描画
				auto DrawLabelFrame = [&](int dx, int dy, QColor const &color){
					painter->setBrush(color);
					painter->drawRoundedRect(r.adjusted((int)lround(dx + 3), (int)lround(dy + 3), (int)lround(dx - 3), (int)lround(dy - 3)), 3, 3);
				};

				QColor color = BranchLabel::color(label.kind); // ラベル表面の色
				QColor hilite = hiliteColor(color); // ハイライトの色
				QColor shadow = shadowColor(color); // 陰の色

				painter->setPen(Qt::NoPen);
				DrawLabelFrame(-1, -1, hilite);
				DrawLabelFrame(1, 1, shadow);
				DrawLabelFrame(0, 0, color);

				// ラベルテキストの描画
				painter->setPen(Qt::black);
				painter->setBrush(Qt::NoBrush);
				QApplication::style()->drawItemText(painter, r.adjusted(space, 0, 0, 0), opt.displayAlignment, opt.palette, true, text);
				x1 = x0;
			}
			painter->restore();
		}
	}

public:
	explicit CommitLogTableWidgetDelegate(CommitLogTableWidget *parent)
		: MyTableWidgetDelegate(parent)
	{
	}
	void paint(QPainter *painter, const QStyleOptionViewItem &option, QModelIndex const &index) const override
	{
		CommitLogTableWidget const *tablewidget = qobject_cast<CommitLogTableWidget const *>(option.widget);
		Q_ASSERT(tablewidget);

		QStyleOptionViewItem opt = option;
		initStyleOption(&opt, index);

		GitCommitItem const &commit = tablewidget->commitItem(index.row());
		CommitRecord const &record = tablewidget->model_->record(index);

		if (tablewidget->model_->isFiltered()) {
			MigemoFilter::fillFilteredBG(painter, opt.rect);
			MigemoFilter::drawText_filted(painter, opt, opt.rect, tablewidget->model_->filter_);
		} else {
			MyTableWidgetDelegate::paint(painter, opt, index);
		}

		enum {
			Graph,
			CommitId,
			Date,
			Author,
			Message,
		};

		const int actual_row = tablewidget->model_->unfilteredIndex(index.row()); // フィルタ適用前の行番号

		// 署名アイコンの描画
		if (index.column() == CommitId) {
			drawSignatureIcon(painter, opt, commit);
		}

		// コミット日時
		if (index.column() == Date) {
			if (commit.order_fixed) {
				QColor color(255, 0, 0, 128);
				QRect r = opt.rect.adjusted(1, 1, -1, -2);
				misc::drawFrame(painter, r.x(), r.y(), r.width(), r.height(), color, color);
			}
		}

		// avatarの描画
		if (index.column() == Author) {
			bool show = global->mainwindow->isAvatarsVisible();
			double opacity = show ? 0.5 : 0.125;
			drawAvatar(painter, opacity, opt, actual_row);
		}

		// ラベルの描画
		if (index.column() == Message) {
			BranchLabelList const &labels = mainwindow()->rowLabels(actual_row);
			QString current_branch = mainwindow()->currentBranchName();
			drawLabels(painter, opt, index, labels, current_branch);
		}
	}
};

GitCommitItem const &CommitLogTableWidget::commitItem(int row) const
{
	return mainwindow()->commitItem(row);
}

CommitLogTableWidget::CommitLogTableWidget(QWidget *parent)
	: QTableView(parent)
{
	setItemDelegate(new CommitLogTableWidgetDelegate(this));
	model_ = new CommitLogTableModel(this);
	setModel(model_);
}

void CommitLogTableWidget::setup(MainWindow *mw)
{
	mainwindow_ = mw;
}

void CommitLogTableWidget::adjustAppearance()
{
	const int n = rowCount();

	for (int i = 0; i < n; i++) {
		setRowHeight(i, 24);
	}

	int t = columnWidth(0);
	resizeColumnsToContents();
	setColumnWidth(0, t);
	horizontalHeader()->setStretchLastSection(false);
	horizontalHeader()->setStretchLastSection(true);
}

void CommitLogTableWidget::setRecords(std::vector<CommitRecord> &&records)
{
	model_->setRecords(std::move(records));

	adjustAppearance();
}

void drawBranch(QPainterPath *path, double x0, double y0, double x1, double y1, double r, bool bend_early)
{
	if (x0 == x1) {
		path->moveTo(x0, y0);
		path->lineTo(x1, y1);
	} else {
		double ym = bend_early ? (y0 + r) : (y1 - r);
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

QRect CommitLogTableWidget::visualItemRect(int row, int col)
{
	QModelIndex index = model_->index(row, col, QModelIndex());
	return visualRect(index);
}

void CommitLogTableWidget::paintEvent(QPaintEvent *e)
{
	if (rowCount() < 1) return;

	QTableView::paintEvent(e);

	QPainter pr(viewport());
	pr.setRenderHint(QPainter::Antialiasing);
	pr.setBrush(QBrush(QColor(255, 255, 255)));

	GitCommitItemList const &list = mainwindow()->commitlog();

	int indent_span = 16;

	int line_width = 2;
	int thick_line_width = 4;

	auto ItemRect = [&](int row){
		return visualItemRect(row, 0);
	};

	auto IsAncestor = [&](GitCommitItem const &item){
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
		if (index < list.size()) {
			GitCommitItem const &item1 = list.at(index);
			rc1 = ItemRect(itemrow);
			QPointF pt1 = ItemPoint(item1.marker_depth, rc1);
			double halfheight = rc1.height() / 2.0;
			for (GitTreeLine const &line : item1.parent_lines) {
				if (line.depth >= 0) {
					QPainterPath *path = nullptr;
					GitCommitItem const &item2 = list.at(line.index);
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
		if (index < list.size()) {
			GitCommitItem const &item = list.at(index);
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

	auto DrawGraph = [&](){
		bool show = global->mainwindow->isGraphVisible(); // グラフ表示モード
		double opacity = show ? 1.0 : 0.125;

		// draw lines

		pr.setOpacity(opacity * 0.5);
		pr.setBrush(Qt::NoBrush);

		for (int i = 0; i < (int)list.size(); i++) {
			double y = DrawLine(i, i);
			if (y >= height()) break;
		}

		// draw marks

		pr.setOpacity(opacity * 1);
		pr.setBrush(mainwindow()->color(0));

		for (int i = 0; i < (int)list.size(); i++) {
			double y = DrawMark(i, i);
			if (y >= height()) break;
		}
	};

	if (!model_->isFiltered()) {
		DrawGraph();
	}
}

void CommitLogTableWidget::resizeEvent(QResizeEvent *e)
{
	mainwindow()->updateAncestorCommitMap();
	QTableView::resizeEvent(e);
}

void CommitLogTableWidget::verticalScrollbarValueChanged(int value)
{
	(void)value;
	mainwindow()->updateAncestorCommitMap();
}

void CommitLogTableWidget::currentChanged(const QModelIndex &current, const QModelIndex &previous)
{
	QTableView::currentChanged(current, previous);
	emit currentRowChanged(current.row());
}

int CommitLogTableWidget::rowCount() const
{
	return model_->rowCount(QModelIndex());
}

int CommitLogTableWidget::currentRow() const
{
	return currentIndex().row();
}

int CommitLogTableWidget::actualLogIndex() const
{
	int row = currentIndex().row();
	if (row >= 0 && row < model_->rowcount()) {
		return model_->unfilteredIndex(row);
	}
	return -1;
}

void CommitLogTableWidget::setCurrentCell(int row, int col)
{
	QModelIndex index = model_->index(row, col, QModelIndex());
	setCurrentIndex(index);
}

void CommitLogTableWidget::updateViewport()
{
	adjustAppearance();
	viewport()->update();
}

void CommitLogTableWidget::setCurrentRow(int row)
{
	QModelIndex index = model_->index(row, 0, QModelIndex());
	setCurrentIndex(index);
}

void CommitLogTableWidget::setFilter(QString const &filter)
{
	if (model_->setFilter(filter)) {
		updateViewport();
		setCurrentRow(0);
	}
}
