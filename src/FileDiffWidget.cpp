#include "FileDiffWidget.h"
#include "MainWindow.h"
#include "ui_FileDiffWidget.h"
#include "misc.h"
#include "GitDiff.h"
#include "joinpath.h"
#include "BigDiffWindow.h"

#include <QBuffer>
#include <QDebug>
#include <QKeyEvent>
#include <QPainter>

enum {
	DiffIndexRole = Qt::UserRole,
};

struct FileDiffWidget::Private {
	MainWindow *mainwindow = nullptr;
	InitParam_ init_param_;
	Git::CommitItemList commit_item_list;
	FileDiffWidget::DiffData diff_data;
	FileDiffWidget::DrawData draw_data;
	int max_line_length = 0;
};

FileDiffWidget::FileDiffWidget(QWidget *parent)
	: QWidget(parent)
	, ui(new Ui::FileDiffWidget)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	pv = new Private();

	setMaximizeButtonEnabled(true);

	ui->widget_diff_left->installEventFilter(this);
	ui->widget_diff_right->installEventFilter(this);
	ui->widget_diff_pixmap->installEventFilter(this);
}

FileDiffWidget::~FileDiffWidget()
{
	delete pv;
	delete ui;
}

void FileDiffWidget::setMaximizeButtonEnabled(bool f)
{
	ui->toolButton_fullscreen->setVisible(f);
	ui->toolButton_fullscreen->setEnabled(f);
}

FileDiffWidget::DiffData *FileDiffWidget::diffdata()
{
	return &pv->diff_data;
}

const FileDiffWidget::DiffData *FileDiffWidget::diffdata() const
{
	return &pv->diff_data;
}

FileDiffWidget::DrawData *FileDiffWidget::drawdata()
{
	return &pv->draw_data;
}

const FileDiffWidget::DrawData *FileDiffWidget::drawdata() const
{
	return &pv->draw_data;
}

void FileDiffWidget::bind(MainWindow *mw)
{
	pv->mainwindow = mw;
	Q_ASSERT(pv->mainwindow);
	ui->widget_diff_pixmap->imbue_(pv->mainwindow, this, diffdata(), drawdata());
	ui->widget_diff_left->imbue_(pv->mainwindow, diffdata(), drawdata());
	ui->widget_diff_right->imbue_(pv->mainwindow, diffdata(), drawdata());

	connect(ui->verticalScrollBar, SIGNAL(valueChanged(int)), this, SLOT(onVerticalScrollValueChanged(int)));
	connect(ui->horizontalScrollBar, SIGNAL(valueChanged(int)), this, SLOT(onHorizontalScrollValueChanged(int)));
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
	*diffdata() = FileDiffWidget::DiffData();
	ui->widget_diff_pixmap->clear(false);
	ui->widget_diff_left->clear(ViewType::Left);
	ui->widget_diff_right->clear(ViewType::Right);
}

int FileDiffWidget::fileviewHeight() const
{
	return ui->widget_diff_left->height();
}

void FileDiffWidget::resetScrollBarValue()
{
	ui->verticalScrollBar->setValue(0);
	ui->horizontalScrollBar->setValue(0);
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

void FileDiffWidget::updateHorizontalScrollBar()
{
	QScrollBar *sb = ui->horizontalScrollBar;
	if (ui->verticalScrollBar->maximum() > 0 && drawdata()->char_width) {
		int chars = ui->widget_diff_left->width() / drawdata()->char_width;
		sb->setRange(0, pv->max_line_length + 10 - chars);
		sb->setPageStep(chars);
		return;
	}
	sb->setRange(0, 0);
	sb->setPageStep(0);
}

void FileDiffWidget::updateSliderCursor()
{
	int total = totalTextLines();
	int value = fileviewScrollPos();
	int size = visibleLines();
	ui->widget_diff_pixmap->setScrollPos(total, value, size);
}

void FileDiffWidget::updateControls()
{
	updateVerticalScrollBar();
	updateHorizontalScrollBar();
	updateSliderCursor();
}

QString FileDiffWidget::formatLine(const QString &text)
{
	if (text.isEmpty()) return text;
	std::vector<ushort> vec;
	vec.reserve(text.size() + 100);
	ushort const *begin = text.utf16();
	ushort const *end = begin + text.size();
	ushort const *ptr = begin;
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

void FileDiffWidget::setDiffText(QList<TextDiffLine> const &left, QList<TextDiffLine> const &right)
{

	QPixmap pm(1, 1);
	QPainter pr(&pm);
	pr.setFont(ui->widget_diff_left->font());
	QFontMetrics fm = pr.fontMetrics();

	pv->max_line_length = 0;

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

			ushort c = item.line.utf16()[0];
			item.line = item.line.mid(1);
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

			item.line = formatLine(item.line);
			out->push_back(item);

			QSize sz = fm.size(0, item.line);
			if (pv->max_line_length < sz.width()) {
				pv->max_line_length = sz.width();
			}
		}
	};
	DO(left, Left, &diffdata()->left_lines);
	DO(right, Right, &diffdata()->right_lines);

	if (drawdata()->char_width > 0) {
		pv->max_line_length /= drawdata()->char_width;
	} else {
		pv->max_line_length = 0;
	}

	ui->widget_diff_left->update(ViewType::Left);
	ui->widget_diff_right->update(ViewType::Right);
}

void FileDiffWidget::init_diff_data_(Git::Diff const &diff)
{
	clearDiffView();
	diffdata()->path = diff.path;
	diffdata()->left_id = diff.blob.a_id;
	diffdata()->right_id = diff.blob.b_id;
}

void FileDiffWidget::prepareSetText_(QByteArray const &ba, Git::Diff const &diff)
{
	init_diff_data_(diff);

	if (ba.isEmpty()) {
		diffdata()->original_lines.clear();
	} else {
		diffdata()->original_lines = misc::splitLines(ba, [](char const *ptr, size_t len){ return QString::fromUtf8(ptr, len); });
	}
}

bool FileDiffWidget::setImage_(QByteArray const &ba, ViewType viewtype)
{
	QString mimetype = pv->mainwindow->determinFileType(ba, true);
	if (misc::isImageFile(mimetype)) {
		QPixmap pixmap;
		pixmap.loadFromData(ba);
		FilePreviewWidget *w = nullptr;
		switch (viewtype) {
		case ViewType::Left:  w = ui->widget_diff_left;  break;
		case ViewType::Right: w = ui->widget_diff_right; break;
		}
		if (w) w->setImage(mimetype, pixmap);
		return true;
	}
	return false;
}

void FileDiffWidget::setTextLeftOnly(QByteArray const &ba, Git::Diff const &diff)
{
	pv->init_param_ = InitParam_();
	pv->init_param_.view_style = FileDiffWidget::ViewStyle::TextLeftOnly;
	pv->init_param_.content_left = ba;
	pv->init_param_.diff = diff;

	if (setImage_(ba, ViewType::Left)) return;

	prepareSetText_(ba, diff);

	QList<TextDiffLine> left_lines;
	QList<TextDiffLine> right_lines;

	for (QString const &line : diffdata()->original_lines) {
		left_lines.push_back('-' + line);
		right_lines.push_back(QString());
	}

	setDiffText(left_lines, right_lines);
}

void FileDiffWidget::setTextRightOnly(QByteArray const &ba, Git::Diff const &diff)
{
	pv->init_param_ = InitParam_();
	pv->init_param_.view_style = FileDiffWidget::ViewStyle::TextRightOnly;
	pv->init_param_.content_left = ba;
	pv->init_param_.diff = diff;

	if (setImage_(ba, ViewType::Right)) return;

	prepareSetText_(ba, diff);

	QList<TextDiffLine> left_lines;
	QList<TextDiffLine> right_lines;

	for (QString const &line : diffdata()->original_lines) {
		left_lines.push_back(QString());
		right_lines.push_back('+' + line);
	}

	setDiffText(left_lines, right_lines);
}

void FileDiffWidget::setTextSideBySide(QByteArray const &ba, Git::Diff const &diff, bool uncommited, QString const &workingdir)
{
	pv->init_param_ = InitParam_();
	pv->init_param_.view_style = FileDiffWidget::ViewStyle::TextSideBySide;
	pv->init_param_.content_left = ba;
	pv->init_param_.diff = diff;
	pv->init_param_.uncommited = uncommited;
	pv->init_param_.workingdir = workingdir;

	if (setImage_(ba, ViewType::Left)) return;

	prepareSetText_(ba, diff);

	if (uncommited) {
		QString path = workingdir / diff.path;
		diffdata()->right_id = GitDiff::prependPathPrefix(path);
	}

	QList<TextDiffLine> left_lines;
	QList<TextDiffLine> right_lines;

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
			for (auto it = tmp_left.rbegin(); it != tmp_left.rend(); it++) left_lines.push_back(*it);
			for (auto it = tmp_right.rbegin(); it != tmp_right.rend(); it++) right_lines.push_back(*it);
			linenum = hi.pos;
			h--;
		}
		if (linenum > 0) {
			linenum--;
			if (linenum < (size_t)diffdata()->original_lines.size()) {
				QString line = ' ' + diffdata()->original_lines[linenum];
				left_lines.push_back(line);
				right_lines.push_back(line);
			}
		}
	}

	std::reverse(left_lines.begin(), left_lines.end());
	std::reverse(right_lines.begin(), right_lines.end());
	setDiffText(left_lines, right_lines);
}

GitPtr FileDiffWidget::git()
{
	return pv->mainwindow->git();
}

QByteArray FileDiffWidget::cat_file(GitPtr g, QString const &id)
{
	QByteArray ba;
	pv->mainwindow->cat_file(id, &ba);
	return ba;
}

bool FileDiffWidget::isValidID_(QString const &id)
{
	if (id.startsWith(PATH_PREFIX)) {
		return true;
	}
	return Git::isValidID(id);
}

void FileDiffWidget::updateDiffView(Git::Diff const &info, bool uncommited)
{
	GitPtr g = git();
	if (!g) return;
	if (!g->isValidWorkingCopy()) return;

	Git::Diff diff;
	if (isValidID_(info.blob.a_id) && isValidID_(info.blob.b_id)) {
		QString text = GitDiff::diffFile(g, info.blob.a_id, info.blob.b_id);
		GitDiff::parseDiff(text, &info, &diff);
	} else {
		diff = info;
	}

	QByteArray ba;
	if (isValidID_(diff.blob.a_id)) { // 左が有効
		ba = cat_file(g, diff.blob.a_id);
		if (isValidID_(diff.blob.b_id)) { // 右が有効
			setTextSideBySide(ba, diff, uncommited, g->workingRepositoryDir()); // 通常のdiff表示
		} else {
			setTextLeftOnly(ba, diff); // 右が無効の時は、削除されたファイル
		}
	} else if (isValidID_(diff.blob.b_id)) { // 左が無効で右が有効の時は、追加されたファイル
		ba = cat_file(g, diff.blob.b_id);
		setTextRightOnly(ba, diff);
	}

	ui->widget_diff_pixmap->clear(false);

	resetScrollBarValue();
	updateControls();

	ui->widget_diff_pixmap->update();
}

void FileDiffWidget::updateDiffView(QString id_left, QString id_right)
{
	GitPtr g = git();
	if (!g) return;
	if (!g->isValidWorkingCopy()) return;

	Git::Diff diff;
	diff.blob.a_id = id_left;
	diff.blob.b_id = id_right;
	diff.mode = "0";
	QString text = GitDiff::diffFile(g, diff.blob.a_id, diff.blob.b_id);
	GitDiff::parseDiff(text, &diff, &diff);

	QByteArray ba = cat_file(g, diff.blob.a_id);
	setTextSideBySide(ba, diff, false, g->workingRepositoryDir());

	ui->widget_diff_pixmap->clear(false);

	resetScrollBarValue();
	updateControls();

	ui->widget_diff_pixmap->update();
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
	drawdata()->v_scroll_pos = value;
	ui->widget_diff_left->update(ViewType::Left);
	ui->widget_diff_right->update(ViewType::Right);
}

void FileDiffWidget::onVerticalScrollValueChanged(int value)
{
	scrollTo(value);
	updateSliderCursor();
}

void FileDiffWidget::onHorizontalScrollValueChanged(int value)
{
	pv->draw_data.h_scroll_pos = value;
	ui->widget_diff_left->update();
	ui->widget_diff_right->update();
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
	updateControls();
}

QPixmap FileDiffWidget::makeDiffPixmap(ViewType side, int width, int height, FileDiffWidget::DiffData const *diffdata, FileDiffWidget::DrawData const *drawdata)
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
			case TextDiffLine::Unknown: return drawdata->bgcolor_gray;
			}
			return QColor();
		});
		Loop([&](TextDiffLine::Type t)->QColor{
			switch (t) {
			case TextDiffLine::Add: return drawdata->bgcolor_add_dark;
			case TextDiffLine::Del: return drawdata->bgcolor_del_dark;
			}
			return QColor();
		});
		if (scale == 1) return pixmap;
		return pixmap.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	};
	if (side == ViewType::Left)  return MakePixmap(diffdata->left_lines, width, height);
	if (side == ViewType::Right) return MakePixmap(diffdata->right_lines, width, height);
	return QPixmap();
}



void FileDiffWidget::on_toolButton_fullscreen_clicked()
{
	BigDiffWindow win(pv->mainwindow);
	win.setWindowState(Qt::WindowMaximized);
	win.init(pv->mainwindow, pv->init_param_);
	win.exec();
}
