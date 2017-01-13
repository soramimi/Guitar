#include "FileDiffWidget.h"
#include "MainWindow.h"
#include "ui_FileDiffWidget.h"
#include "misc.h"
#include "GitDiff.h"
#include "joinpath.h"

#include <QKeyEvent>
#include <QPainter>

enum {
	DiffIndexRole = Qt::UserRole,
};

FileDiffWidget::FileDiffWidget(QWidget *parent)
	: QWidget(parent)
	, ui(new Ui::FileDiffWidget)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	ui->widget_diff_left->installEventFilter(this);
	ui->widget_diff_right->installEventFilter(this);
	ui->widget_diff_pixmap->installEventFilter(this);

	ui->horizontalScrollBar->setVisible(false);
}

FileDiffWidget::~FileDiffWidget()
{
	delete ui;
}

void FileDiffWidget::bind(MainWindow *mw)
{
	mainwindow = mw;
	Q_ASSERT(mainwindow);
	ui->widget_diff_pixmap->imbue_(mainwindow, this, &data_);
	ui->widget_diff_left->imbue_(&data_);
	ui->widget_diff_right->imbue_(&data_);

	connect(ui->verticalScrollBar, SIGNAL(valueChanged(int)), this, SLOT(onScrollValueChanged(int)));
	connect(ui->widget_diff_pixmap, SIGNAL(scrollByWheel(int)), this, SLOT(onDiffWidgetWheelScroll(int)));
	connect(ui->widget_diff_pixmap, SIGNAL(valueChanged(int)), this, SLOT(onScrollValueChanged2(int)));
	connect(ui->widget_diff_left, SIGNAL(scrollByWheel(int)), this, SLOT(onDiffWidgetWheelScroll(int)));
	connect(ui->widget_diff_left, SIGNAL(resized()), this, SLOT(onDiffWidgetResized()));
	connect(ui->widget_diff_right, SIGNAL(scrollByWheel(int)), this, SLOT(onDiffWidgetWheelScroll(int)));
	connect(ui->widget_diff_right, SIGNAL(resized()), this, SLOT(onDiffWidgetResized()));

	int top_margin = 1;
	int bottom_margin = 1;
	ui->widget_diff_left->updateDrawData_(top_margin, bottom_margin);
}

void FileDiffWidget::clearDiffView()
{
	*diffdata() = DiffWidgetData::DiffData();
	ui->widget_diff_pixmap->clear(false);
	ui->widget_diff_left->clear(ViewType::Left);
	ui->widget_diff_right->clear(ViewType::Right);
}

int FileDiffWidget::fileviewHeight() const
{
	return ui->widget_diff_left->height();
}

QString FileDiffWidget::formatLine(const QString &text, bool diffmode)
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

void FileDiffWidget::setVerticalScrollBarValue(int pos)
{
	ui->verticalScrollBar->setValue(pos);
}

void FileDiffWidget::updateVerticalScrollBar()
{
	QScrollBar *sb = ui->verticalScrollBar;
	if (drawdata()->line_height > 0) {
		int lines_per_widget = fileviewHeight() / drawdata()->line_height;
		if (lines_per_widget < diffdata()->left_lines.size() + 1) {
			sb->setRange(0, diffdata()->left_lines.size() - lines_per_widget + 1);
			sb->setPageStep(lines_per_widget);
			return;
		}
	}
	sb->setRange(0, 0);
	sb->setPageStep(0);
}

void FileDiffWidget::setDiffText_(QList<TextDiffLine> const &left, QList<TextDiffLine> const &right, bool diffmode)
{
	enum Pane {
		Left,
		Right,
	};
	auto DO = [&](QList<TextDiffLine> const &lines, Pane pane, QList<TextDiffLine> *out){
		out->clear();
		int linenum = 0;
		for (TextDiffLine const &line : lines) {
			TextDiffLine item = line;
			item.type = TextDiffLine::Unknown;
			if (diffmode) {
				ushort c = item.line.utf16()[0];
				if (c == ' ') {
					item.type = TextDiffLine::Unchanged;
					item.line_number = linenum++;
				} else if (c == '+') {
					item.type = TextDiffLine::Add;
					if (pane == Right) {
						item.line_number = linenum++;
					}
				} else if (c == '-') {
					item.type = TextDiffLine::Del;
					if (pane == Left) {
						item.line_number = linenum++;
					}
				}
			}
			item.line = formatLine(item.line, diffmode);
			out->push_back(item);
		}
	};
	DO(left, Left, &diffdata()->left_lines);
	DO(right, Right, &diffdata()->right_lines);

	ui->widget_diff_left->update(ViewType::Left);
	ui->widget_diff_right->update(ViewType::Right);
}

void FileDiffWidget::init_diff_data_(Git::Diff const &diff)
{
	clearDiffView();
	diffdata()->path = diff.path;
	diffdata()->left = diff.blob.a;
	diffdata()->right = diff.blob.b;
}



void FileDiffWidget::setDataAsNewFile(QByteArray const &ba, Git::Diff const &diff)
{
	init_diff_data_(diff);

	if (ba.isEmpty()) {
		diffdata()->original_lines.clear();
	} else {
		diffdata()->original_lines = misc::splitLines(ba, [](char const *ptr, size_t len){ return QString::fromUtf8(ptr, len); });
	}

	QList<TextDiffLine> left_newlines;
	QList<TextDiffLine> right_newlines;

	for (QString const &line : diffdata()->original_lines) {
		QString text = '+' + line;
		left_newlines.push_back(QString());
		right_newlines.push_back(text);
	}

	setDiffText_(left_newlines, right_newlines, true);
}

void FileDiffWidget::setTextDiffData(QByteArray const &ba, Git::Diff const &diff, bool uncommited, QString const &workingdir)
{
	init_diff_data_(diff);

	if (uncommited) {
		QString path = workingdir / diff.path;
		diffdata()->right.id = GitDiff::prependPathPrefix(path);
	}

	if (ba.isEmpty()) {
		diffdata()->original_lines.clear();
	} else {
		diffdata()->original_lines = misc::splitLines(ba, [](char const *ptr, size_t len){ return QString::fromUtf8(ptr, len); });
	}

	QList<TextDiffLine> left_newlines;
	QList<TextDiffLine> right_newlines;

	size_t linenum = diffdata()->original_lines.size();

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
			std::vector<TextDiffLine> tmp_left;
			std::vector<TextDiffLine> tmp_right;
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
					TextDiffLine l(line);
					l.hunk_number = hunk_number;
					tmp_left.push_back(l);
				} else if (c == '+') {
					plus++;
					TextDiffLine l(line);
					l.hunk_number = hunk_number;
					tmp_right.push_back(l);
				} else {
					FlushBlank();
					TextDiffLine l(line);
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
			if (linenum < (size_t)diffdata()->original_lines.size()) {
				QString line = ' ' + diffdata()->original_lines[linenum];
				left_newlines.push_back(line);
				right_newlines.push_back(line);
			}
		}
	}

	std::reverse(left_newlines.begin(), left_newlines.end());
	std::reverse(right_newlines.begin(), right_newlines.end());
	setDiffText_(left_newlines, right_newlines, true);
}

void FileDiffWidget::setPath(QString const &path)
{
	this->path = path;
}

GitPtr FileDiffWidget::git()
{
	return mainwindow->git();
}

void FileDiffWidget::updateDiffView(Git::Diff const &info, bool uncommited)
{
	GitPtr g = git();
	if (!g) return;
	if (!g->isValidWorkingCopy()) return;

	Git::Diff diff;
	if (info.blob.a.id.isEmpty()) { // 左が空（新しく追加されたファイル）
		diff = info;
	} else {
		QString text = GitDiff::diffFile(g, info.blob.a.id, info.blob.b.id);
		GitDiff::parseDiff(text, &info, &diff);
	}

	QByteArray ba;
	if (diff.blob.a.id.isEmpty()) {
		mainwindow->cat_file(g, diff.blob.b.id, &ba);
		setDataAsNewFile(ba, diff);
	} else {
		mainwindow->cat_file(g, diff.blob.a.id, &ba);
		setTextDiffData(ba, diff, uncommited, g->workingRepositoryDir());
	}

	ui->widget_diff_pixmap->clear(false);

	setVerticalScrollBarValue(0);
	updateVerticalScrollBar();
	updateSliderCursor();
}

void FileDiffWidget::updateDiffView(QString id_left, QString id_right)
{
	GitPtr g = git();
	if (!g) return;
	if (!g->isValidWorkingCopy()) return;

	Git::Diff diff;
	diff.path = path;
	diff.blob.a.id = id_left;
	diff.blob.b.id = id_right;
	diff.mode = "0";
	QString text = GitDiff::diffFile(g, diff.blob.a.id, diff.blob.b.id);
	GitDiff::parseDiff(text, &diff, &diff);

	QByteArray ba;
	mainwindow->cat_file(g, diff.blob.a.id, &ba);
	setTextDiffData(ba, diff, false, g->workingRepositoryDir());

	ui->widget_diff_pixmap->clear(false);

	ui->verticalScrollBar->setValue(0);
	updateVerticalScrollBar();
	updateSliderCursor();

	ui->widget_diff_pixmap->update();
}

void FileDiffWidget::updateSliderCursor()
{
	int total = totalTextLines();
	int value = fileviewScrollPos();
	int size = visibleLines();
	ui->widget_diff_pixmap->setScrollPos(total, value, size);
}

bool FileDiffWidget::eventFilter(QObject *watched, QEvent *event)
{
	if (event->type() == QEvent::KeyPress) {
		QKeyEvent *e = dynamic_cast<QKeyEvent *>(event);
		Q_ASSERT(e);
		int k = e->key();
		if (watched == ui->widget_diff_left || watched == ui->widget_diff_right || watched == ui->widget_diff_pixmap) {
			QScrollBar::SliderAction act = QScrollBar::SliderNoAction;
			switch (k) {
			case Qt::Key_Up:       act = QScrollBar::SliderSingleStepSub; break;
			case Qt::Key_Down:     act = QScrollBar::SliderSingleStepAdd; break;
			case Qt::Key_PageUp:   act = QScrollBar::SliderPageStepSub;   break;
			case Qt::Key_PageDown: act = QScrollBar::SliderPageStepAdd;   break;
			case Qt::Key_Home:     act = QScrollBar::SliderToMinimum;     break;
			case Qt::Key_End:      act = QScrollBar::SliderToMaximum;     break;
			}
			if (act != QScrollBar::SliderNoAction) {
				ui->verticalScrollBar->triggerAction(act);
				return true;
			}
		}
	}
	return false;
}

void FileDiffWidget::scrollTo(int value)
{
	drawdata()->scrollpos = value;
	ui->widget_diff_left->update(ViewType::Left);
	ui->widget_diff_right->update(ViewType::Right);
}

void FileDiffWidget::onScrollValueChanged(int value)
{
	scrollTo(value);

	updateSliderCursor();
}

void FileDiffWidget::onDiffWidgetWheelScroll(int lines)
{
	while (lines > 0) {
		ui->verticalScrollBar->triggerAction(QScrollBar::SliderSingleStepAdd);
		lines--;
	}
	while (lines < 0) {
		ui->verticalScrollBar->triggerAction(QScrollBar::SliderSingleStepSub);
		lines++;
	}
}

void FileDiffWidget::onScrollValueChanged2(int value)
{
	ui->verticalScrollBar->setValue(value);
}

void FileDiffWidget::onDiffWidgetResized()
{
	updateSliderCursor();
	updateVerticalScrollBar();
}


QPixmap FileDiffWidget::makeDiffPixmap_(ViewType side, int width, int height, DiffWidgetData const *dd)
{
	auto MakePixmap = [&](QList<TextDiffLine> const &lines, int w, int h){
		const int scale = 1;
		QPixmap pixmap = QPixmap(w, h * scale);
		pixmap.fill(Qt::white);
		QPainter pr(&pixmap);
		auto Loop = [&](std::function<QColor(TextDiffLine::Type)> getcolor){
			int i = 0;
			while (i < lines.size()) {
				TextDiffLine::Type type = lines[i].type;
				int j = i + 1;
				if (type != TextDiffLine::Unchanged) {
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
		Loop([&](TextDiffLine::Type t)->QColor{
			switch (t) {
			case TextDiffLine::Unknown: return dd->drawdata.bgcolor_gray;
			}
			return QColor();
		});
		Loop([&](TextDiffLine::Type t)->QColor{
			switch (t) {
			case TextDiffLine::Add: return dd->drawdata.bgcolor_add_dark;
			case TextDiffLine::Del: return dd->drawdata.bgcolor_del_dark;
			}
			return QColor();
		});
		if (scale == 1) return pixmap;
		return pixmap.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	};
	if (side == ViewType::Left)  return MakePixmap(dd->diffdata.left_lines, width, height);
	if (side == ViewType::Right) return MakePixmap(dd->diffdata.right_lines, width, height);
	return QPixmap();
}

QPixmap FileDiffWidget::makeDiffPixmap(ViewType side, int width, int height)
{
	return makeDiffPixmap_(side, width, height, getDiffWidgetData());
}
