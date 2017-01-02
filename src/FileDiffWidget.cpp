#include "FileDiffWidget.h"
#include "FileDiffSliderWidget.h"

#include "MainWindow.h"

#include <QDebug>
#include <QFileDialog>
#include <QMenu>
#include <QPainter>
#include <QWheelEvent>
#include <functional>
#include "misc.h"
#include "joinpath.h"

#define PATH_PREFIX '*'

struct FileDiffWidget::Line {
	enum Type {
		Unknown,
		Unchanged,
		Add,
		Del,
	} type;
	int hunk_number = -1;
	int line_number = -1;
	QString line;
	Line()
	{
	}
	Line(QString const &text)
		: line(text)
	{
	}
};

struct FileDiffWidget::Private {
	QScrollBar *vertical_scroll_bar = nullptr;
	struct Data {
		QStringList original_lines;
		QList<Line> left_lines;
		QList<Line> right_lines;
		Git::BLOB left;
		Git::BLOB right;
	} data;
	int scrollpos = 0;
	int char_width = 0;
	int line_height = 0;
	QColor bgcolor_text;
	QColor bgcolor_add;
	QColor bgcolor_del;
	QColor bgcolor_add_dark;
	QColor bgcolor_del_dark;
	QColor bgcolor_gray;
	FileDiffWidget::Side forcus = FileDiffWidget::Side::None;
};


FileDiffWidget::FileDiffWidget(QWidget *parent)
	: QWidget(parent)
{
	pv = new Private();
	pv->scrollpos = 0;

	pv->bgcolor_text = QColor(255, 255, 255);
	pv->bgcolor_gray = QColor(224, 224, 224);
	pv->bgcolor_add = QColor(192, 240, 192);
	pv->bgcolor_del = QColor(255, 224, 224);
	pv->bgcolor_add_dark = QColor(64, 192, 64);
	pv->bgcolor_del_dark = QColor(240, 64, 64);


#if defined(Q_OS_WIN32)
	setFont(QFont("MS Gothic"));
#elif defined(Q_OS_LINUX)
	setFont(QFont("Monospace"));
#elif defined(Q_OS_MAC)
	setFont(QFont("Menlo"));
#endif
}


FileDiffWidget::~FileDiffWidget()
{
	delete pv;
}

void FileDiffWidget::updateVerticalScrollBar(QScrollBar *sb)
{
	pv->vertical_scroll_bar = sb;
	if (sb) {
		if (pv->line_height > 0) {
			int lines_per_widget = height() / pv->line_height;
			if (lines_per_widget < pv->data.left_lines.size() + 1) {
				sb->setRange(0, pv->data.left_lines.size() - lines_per_widget + 1);
				sb->setPageStep(lines_per_widget);
				return;
			}
		}
		sb->setRange(0, 0);
		sb->setPageStep(0);
	}
}

void FileDiffWidget::clear()
{
	pv->data = Private::Data();
	update();
}

QString FileDiffWidget::formatLine(QString const &text, bool diffmode)
{
	if (text.isEmpty()) return text;
	std::vector<ushort> vec;
	vec.reserve(text.size() + 100);
	ushort const *begin = text.utf16();
	ushort const *end = begin + text.size();
	ushort const *ptr = begin;
	if (diffmode) {
		vec.push_back(*ptr);
		ptr++;
	}
	int x = 0;
	while (ptr < end) {
		if (*ptr == '\t') {
			do {
				vec.push_back(' ');
				x++;
			} while ((x % 4) != 0);
			ptr++;
		} else {
			vec.push_back(*ptr);
			ptr++;
			x++;
		}
	}
	return QString::fromUtf16(&vec[0], vec.size());
}

void FileDiffWidget::setText_(QList<Line> const &left, QList<Line> const &right, bool diffmode)
{
	enum Pane {
		Left,
		Right,
	};
	auto DO = [&](QList<Line> const &lines, Pane pane, QList<Line> *out){
		out->clear();
		int linenum = 0;
		for (Line const &line : lines) {
			Line item = line;
			item.type = Line::Unknown;
			if (diffmode) {
				ushort c = item.line.utf16()[0];
				if (c == ' ') {
					item.type = Line::Unchanged;
					item.line_number = linenum++;
				} else if (c == '+') {
					item.type = Line::Add;
					if (pane == Right) {
						item.line_number = linenum++;
					}
				} else if (c == '-') {
					item.type = Line::Del;
					if (pane == Left) {
						item.line_number = linenum++;
					}
				}
			}
			item.line = formatLine(item.line, diffmode);
			out->push_back(item);
		}
	};
	DO(left, Left, &pv->data.left_lines);
	DO(right, Right, &pv->data.right_lines);

	update();
}

QPixmap FileDiffWidget::makePixmap(Side side, int width, int height)
{
	auto MakePixmap = [&](QList<Line> const &lines, int w, int h){
		const int scale = 1;
		QPixmap pixmap = QPixmap(w, h * scale);
		pixmap.fill(Qt::white);
		QPainter pr(&pixmap);
		auto Loop = [&](std::function<QColor(FileDiffWidget::Line::Type)> getcolor){
			int i = 0;
			while (i < lines.size()) {
				Line::Type type = lines[i].type;
				int j = i + 1;
				if (type != Line::Unchanged) {
					while (j < lines.size()) {
						if (lines[j].type != type) break;
						j++;
					}
					int y = i * pixmap.height() / lines.size();
					int z = j * pixmap.height() / lines.size();
					if (z == y) z = y + 1;
					QColor color = getcolor(type);
					if (color.isValid()) pr.fillRect(0, y, w, z - y, color);
				}
				i = j;
			}
		};
		Loop([&](Line::Type t){
			switch (t) {
			case Line::Unknown: return pv->bgcolor_gray;
			}
			return QColor();
		});
		Loop([&](Line::Type t){
			switch (t) {
			case Line::Add: return pv->bgcolor_add_dark;
			case Line::Del: return pv->bgcolor_del_dark;
			}
			return QColor();
		});
		if (scale == 1) return pixmap;
		return pixmap.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	};
	if (side == Side::Left)  return MakePixmap(pv->data.left_lines, width, height);
	if (side == Side::Right) return MakePixmap(pv->data.right_lines, width, height);
	return QPixmap();
}

void FileDiffWidget::setDataAsNewFile(QByteArray const &ba)
{
	clear();

	if (ba.isEmpty()) {
		pv->data.original_lines.clear();
	} else {
		pv->data.original_lines = misc::splitLines(ba, [](char const *ptr, size_t len){ return QString::fromUtf8(ptr, len); });
	}

	QList<Line> left_newlines;
	QList<Line> right_newlines;

	for (QString const &line : pv->data.original_lines) {
		QString text = '+' + line;
		left_newlines.push_back(QString());
		right_newlines.push_back(text);
	}

	setText_(left_newlines, right_newlines, true);
}

void FileDiffWidget::setData(QByteArray const &ba, Git::Diff const &diff, bool uncmmited, QString const &workingdir)
{
	clear();
	pv->data.left = diff.blob.a;
	pv->data.right = diff.blob.b;
	if (uncmmited) {
		QString path = workingdir / pv->data.right.path;
		pv->data.right.id = QString(PATH_PREFIX) + path;
	}

	if (ba.isEmpty()) {
		pv->data.original_lines.clear();
	} else {
		pv->data.original_lines = misc::splitLines(ba, [](char const *ptr, size_t len){ return QString::fromUtf8(ptr, len); });
	}

	QList<Line> left_newlines;
	QList<Line> right_newlines;

	size_t linenum = pv->data.original_lines.size();

	std::vector<HunkItem> hunks;
	int number = 0;
	for (auto it = diff.hunks.begin(); it != diff.hunks.end(); it++, number++) {
		QString at = it->at;
		if (at.startsWith("@@ -")) {
			size_t pos = 0;
			size_t len = 0;
			ushort const *p = at.utf16() + 4;
			auto ParseNumber = [&](){
				size_t v = 0;
				while (QChar::isDigit(*p)) {
					v = v * 10 + (*p - '0');
					p++;
				}
				return v;
			};
			pos = ParseNumber();
			if (*p == ',') {
				p++;
				len = ParseNumber();
			} else {
				len = 1;
			}
			if (pos > 0) pos--;
			HunkItem item;
			item.hunk_number = number;
			item.pos = pos;
			item.len = len;
			for (QString const &line : it->lines) {
				item.lines.push_back(line);
			}
			hunks.push_back(item);
		}
	}
	std::sort(hunks.begin(), hunks.end(), [](HunkItem const &l, HunkItem const &r){
		return l.pos + l.len < r.pos + r.len;
	});
	size_t h = hunks.size();
	while (linenum > 0 || h > 0) {
		while (h > 0) {
			int hunk_number = h - 1;
			HunkItem const &hi = hunks[hunk_number];
			if (hi.pos + hi.len < linenum) {
				break;
			}
			std::vector<Line> tmp_left;
			std::vector<Line> tmp_right;
			int minus = 0;
			int plus = 0;
			auto FlushBlank = [&](){
				while (minus < plus) {
					tmp_left.push_back(QString());
					minus++;
				}
				while (minus > plus) {
					tmp_right.push_back(QString());
					plus++;
				}
				minus = plus = 0;
			};
			for (auto it = hi.lines.begin(); it != hi.lines.end(); it++) {
				QString line = *it;
				ushort c = line.utf16()[0];
				if (c == '-') {
					minus++;
					Line l(line);
					l.hunk_number = hunk_number;
					tmp_left.push_back(l);
				} else if (c == '+') {
					plus++;
					Line l(line);
					l.hunk_number = hunk_number;
					tmp_right.push_back(l);
				} else {
					FlushBlank();
					Line l(line);
					l.hunk_number = hunk_number;
					tmp_left.push_back(l);
					tmp_right.push_back(l);
				}
			}
			FlushBlank();
			for (auto it = tmp_left.rbegin(); it != tmp_left.rend(); it++) left_newlines.push_back(*it);
			for (auto it = tmp_right.rbegin(); it != tmp_right.rend(); it++) right_newlines.push_back(*it);
			linenum = hi.pos;
			h--;
		}
		if (linenum > 0) {
			linenum--;
			if (linenum < (size_t)pv->data.original_lines.size()) {
				QString line = ' ' + pv->data.original_lines[linenum];
				left_newlines.push_back(line);
				right_newlines.push_back(line);
			}
		}
	}

	std::reverse(left_newlines.begin(), left_newlines.end());
	std::reverse(right_newlines.begin(), right_newlines.end());
	setText_(left_newlines, right_newlines, true);
}

int FileDiffWidget::lineCount() const
{
	return pv->data.left_lines.size();
}

void FileDiffWidget::scrollTo(int value)
{
	pv->scrollpos = value;
	update();
}

int FileDiffWidget::scrollPos() const
{
	return pv->scrollpos;
}

int FileDiffWidget::totalLines() const
{
	return pv->data.left_lines.size();
}

int FileDiffWidget::visibleLines() const
{
	int n = 0;
	if (pv->line_height > 0) {
		n = height() / pv->line_height;
		if (n < 1) n = 1;
	}
	return n;
}

void FileDiffWidget::paintEvent(QPaintEvent *)
{
	QPainter pr(this);

	int x;
	int w = width() / 2;
	int h = height();
	int top_margin = 1;
	int bottom_margin = 1;

	QFontMetrics fm = pr.fontMetrics();
	int descent = fm.descent();
	QSize sz = fm.size(0, "W");
	pv->char_width = sz.width();
	pv->line_height = sz.height() + top_margin + bottom_margin;

	auto Draw = [&](QList<Line> const &lines){
		int linenum_w = sz.width() * 5 + 1;
		int x2 = x + linenum_w;
		int w2 = w - linenum_w;
		if (w2 < 1) return;
		pr.save();
		pr.setClipRect(x, 0, w, h);
		int y = pv->scrollpos * pv->line_height;
		int i = y / pv->line_height;
		y -= i * pv->line_height;
		y = -y;
		while (i < lines.size() && y < h) {
			QString const &line = lines[i].line;
			QColor *bgcolor;
			switch (lines[i].type) {
			case Line::Unchanged:
				bgcolor = &pv->bgcolor_text;
				break;
			case Line::Add:
				bgcolor = &pv->bgcolor_add;
				break;
			case Line::Del:
				bgcolor = &pv->bgcolor_del;
				break;
			default:
				bgcolor = &pv->bgcolor_gray;
				break;
			}
			int line_y = y + pv->line_height - descent - bottom_margin;
			{
				pr.fillRect(x, y, linenum_w + pv->char_width, pv->line_height, pv->bgcolor_gray);
				int num = lines[i].line_number;
				if (num >= 0 && num < lines.size()) {
					QString text = "     " + QString::number(num + 1);
					text = text.mid(text.size() - 5);
					pr.setPen(QColor(128, 128, 128));
					pr.drawText(x, line_y, text);
				}

			}
			pr.fillRect(x2 + pv->char_width, y, w2 - pv->char_width, pv->line_height, QBrush(*bgcolor));
#if 0
			if (lines[i].type == Line::Unknown) {
				QBrush br2(QColor(192, 192, 192), Qt::Dense6Pattern);
				pr.setBrush(br2);
				pr.fillRect(x2 + pv->char_width, y, w2 - pv->char_width, pv->line_height, br2);
			}
#endif
			pr.setPen(QColor(0, 0, 0));
			pr.drawText(x2, line_y, line);
			y += pv->line_height;
			i++;
		}
		if (y < h) {
			pr.fillRect(x, y, w, h - y, QColor(192, 192, 192));
			pr.fillRect(x, y, w, 1, Qt::black);
		}
		pr.fillRect(x, 0, 1, h, QColor(160, 160, 160));
		pr.restore();
	};
	x = 0;
	Draw(pv->data.left_lines);
	x = w;
	w = width() - x;
	Draw(pv->data.right_lines);

	if (pv->forcus != Side::None) {
		int x = 0;
		int y = 0;
		int w = width();
		int h = height();
		if (pv->forcus == Side::Left) {
			w /= 2;
		} else if (pv->forcus == Side::Right) {
			x = w / 2;
			w -= x;
		} else {
			w = 0;
			h = 0;
		}
		if (w > 0 && h > 0) {
			QColor color(0, 128, 255, 32);
			pr.fillRect(x, y, w, h, color);
		}
	}
}

void FileDiffWidget::wheelEvent(QWheelEvent *e)
{
	int delta = e->delta();
	if (delta < 0) {
		delta = -delta / 40;
		if (delta == 0) delta = 1;
		emit scrollByWheel(delta);
	} else if (delta > 0) {
		delta /= 40;
		if (delta == 0) delta = 1;
		emit scrollByWheel(-delta);

	}
}

void FileDiffWidget::resizeEvent(QResizeEvent *)
{
	updateVerticalScrollBar(pv->vertical_scroll_bar);
	emit resized();
}

void FileDiffWidget::contextMenuEvent(QContextMenuEvent *)
{
	QPoint pos = QCursor::pos();

	Side side = mapFromGlobal(pos).x() < width() / 2 ? Side::Left : Side::Right;

	Git::BLOB blob;
	switch (side) {
	case Side::Left:  blob = pv->data.left;  break;
	case Side::Right: blob = pv->data.right; break;
	}

	QMenu menu;
	QAction *a_save_as = blob.id.isEmpty() ? nullptr : menu.addAction(tr("Save as..."));
	if (!menu.actions().isEmpty()) {
		pv->forcus = side;
		update();
		QAction *a = menu.exec(pos + QPoint(8, -8));
		if (a) {
			if (a == a_save_as) {
				if (!blob.id.isEmpty()) {
					blob.path = QFileDialog::getSaveFileName(window(), tr("Save as"), blob.path);
					if (!blob.path.isEmpty()) {
						MainWindow *mw = qobject_cast<MainWindow *>(window());
						if (blob.id.startsWith(PATH_PREFIX)) {
							mw->saveFileAs(blob.id.mid(1), blob.path);
						} else {
							mw->saveBlobAs(blob.id, blob.path);
						}
					}
				}
			}
		}
		pv->forcus = Side::None;
		update();
	}

}

