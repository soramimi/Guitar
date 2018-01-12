#include "FileDiffWidget.h"
#include "ui_FileDiffWidget.h"

#include "BigDiffWindow.h"
#include "GitDiff.h"
#include "common/joinpath.h"
#include "MainWindow.h"
#include "common/misc.h"

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
	std::vector<std::string> original_lines;
	TextEditorEnginePtr engine_left;
	TextEditorEnginePtr engine_right;
	TextDiffLineList left_lines;
	TextDiffLineList right_lines;
	int max_line_length = 0;

	int term_cursor_row = 0;
	int term_cursor_col = 0;
};

FileDiffWidget::FileDiffWidget(QWidget *parent)
	: QWidget(parent)
	, ui(new Ui::FileDiffWidget)
	, m(new Private)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	ui->widget_diff_slider->bind(this);
	connect(ui->widget_diff_slider, SIGNAL(valueChanged(int)), this, SLOT(scrollTo(int)));
	connect(ui->widget_diff_left->texteditor(), SIGNAL(moved(int,int,int,int)), this, SLOT(onMoved(int,int,int,int)));
	connect(ui->widget_diff_right->texteditor(), SIGNAL(moved(int,int,int,int)), this, SLOT(onMoved(int,int,int,int)));

	setFocusAcceptable(true);

	int n = style()->pixelMetric(QStyle::PM_ScrollBarExtent);
	ui->toolButton_fullscreen->setFixedSize(n, n);

	setMaximizeButtonEnabled(true);

	m->engine_left = TextEditorEnginePtr(new TextEditorEngine);
	m->engine_right = TextEditorEnginePtr(new TextEditorEngine);
	ui->widget_diff_left->setDiffMode(m->engine_left, ui->verticalScrollBar, ui->horizontalScrollBar);
	ui->widget_diff_right->setDiffMode(m->engine_right, ui->verticalScrollBar, ui->horizontalScrollBar);

	setViewType(FileViewType::None);
}

FileDiffWidget::~FileDiffWidget()
{
	delete m;
	delete ui;
}

void FileDiffWidget::setViewType(FileViewType type)
{
	ui->widget_diff_left->setViewType(type);
	ui->widget_diff_right->setViewType(type);
}

void FileDiffWidget::setMaximizeButtonEnabled(bool f)
{
	ui->toolButton_fullscreen->setVisible(f);
	ui->toolButton_fullscreen->setEnabled(f);
}

FileDiffWidget::ViewStyle FileDiffWidget::viewstyle() const
{
	return m->init_param_.view_style;
}



void FileDiffWidget::bind(MainWindow *mw)
{
	Q_ASSERT(mw);
	m->mainwindow = mw;
	ui->widget_diff_left->bind(mw, this, ui->verticalScrollBar, ui->horizontalScrollBar, mw->themeForTextEditor());
	ui->widget_diff_right->bind(mw, this, ui->verticalScrollBar, ui->horizontalScrollBar, mw->themeForTextEditor());

	connect(ui->verticalScrollBar, SIGNAL(valueChanged(int)), this, SLOT(onVerticalScrollValueChanged(int)));
	connect(ui->horizontalScrollBar, SIGNAL(valueChanged(int)), this, SLOT(onHorizontalScrollValueChanged(int)));
}

GitPtr FileDiffWidget::git()
{
	return m->mainwindow->git();
}

Git::Object FileDiffWidget::cat_file(GitPtr /*g*/, QString const &id)
{
	return m->mainwindow->cat_file(id);
}

int FileDiffWidget::totalTextLines() const
{
	return m->engine_left->document.lines.size();
}

void FileDiffWidget::clearDiffView()
{
	ui->widget_diff_slider->clear(false);
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

void FileDiffWidget::scrollToBottom()
{
	QScrollBar *sb = ui->verticalScrollBar;
	sb->setValue(sb->maximum());
}

void FileDiffWidget::updateSliderCursor()
{
	if (viewstyle() == SideBySideText) {
		ui->widget_diff_slider->clear(true);
	}
}

void FileDiffWidget::updateControls()
{
	updateSliderCursor();
}

void FileDiffWidget::makeSideBySideDiffData(Git::Diff const &diff, std::vector<std::string> const &original_lines, TextDiffLineList *left_lines, TextDiffLineList *right_lines)
{
	left_lines->clear();
	right_lines->clear();

	m->original_lines = original_lines;

	size_t linenum = original_lines.size();

	std::vector<HunkItem> hunks;
	int number = 0;
	for (auto it = diff.hunks.begin(); it != diff.hunks.end(); it++, number++) {
		std::string at = it->at;
		if (strncmp(at.c_str(), "@@ -", 4) == 0) {
			size_t pos = 0;
			size_t len = 0;
			char const *p = at.c_str() + 4;
			auto ParseNumber = [&](){
				size_t v = 0;
				while (isdigit(*p & 0xff)) {
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
			for (std::string const &line : it->lines) {
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
					tmp_left.push_back(TextDiffLine());
					minus++;
				}
				while (minus > plus) {
					tmp_right.push_back(TextDiffLine());
					plus++;
				}
				minus = plus = 0;
			};
			for (auto it = hi.lines.begin(); it != hi.lines.end(); it++) {
				std::string line = *it;
				int c = line[0] & 0xff;
				line = line.substr(1);
				if (c == '-') {
					minus++;
					TextDiffLine l(line, TextDiffLine::Del);
					l.hunk_number = hunk_number;
					tmp_left.push_back(l);
				} else if (c == '+') {
					plus++;
					TextDiffLine l(line, TextDiffLine::Add);
					l.hunk_number = hunk_number;
					tmp_right.push_back(l);
				} else {
					FlushBlank();
					TextDiffLine l(line, TextDiffLine::Normal);
					l.hunk_number = hunk_number;
					tmp_left.push_back(l);
					tmp_right.push_back(l);
				}
			}
			FlushBlank();
			auto ComplementNewLine = [](std::vector<TextDiffLine> *lines){
				for (TextDiffLine &line : *lines) {
					int n = line.text.size();
					if (n > 0) {
						int c = line.text[n - 1] & 0xff;
						if (c != '\r' && c != '\n') {
							line.text.push_back('\n');
						}
					}
				}
			};
			ComplementNewLine(&tmp_left);
			ComplementNewLine(&tmp_right);
			for (auto it = tmp_left.rbegin(); it != tmp_left.rend(); it++) left_lines->push_back(*it);
			for (auto it = tmp_right.rbegin(); it != tmp_right.rend(); it++) right_lines->push_back(*it);
			linenum = hi.pos;
			h--;
		}
		if (linenum > 0) {
			linenum--;
			if (linenum < (size_t)original_lines.size()) {
				std::string line = original_lines[linenum];
				left_lines->push_back(TextDiffLine(line, TextDiffLine::Normal));
				right_lines->push_back(TextDiffLine(line, TextDiffLine::Normal));
			}
		}
	}

	std::reverse(left_lines->begin(), left_lines->end());
	std::reverse(right_lines->begin(), right_lines->end());
}

void FileDiffWidget::setDiffText(Git::Diff const &diff, TextDiffLineList const &left, TextDiffLineList const &right)
{
	m->max_line_length = 0;

	enum Pane {
		Left,
		Right,
	};
	auto SetLineNumber = [&](TextDiffLineList const &lines, Pane pane, TextDiffLineList *out){
		out->clear();
		int linenum = 1;
		for (TextDiffLine const &line : lines) {
			TextDiffLine item = line;

			switch (item.type) {
			case TextDiffLine::Normal:
				item.line_number = linenum++;
				break;
			case TextDiffLine::Add:
				if (pane == Pane::Right) {
					item.line_number = linenum++;
				}
				break;
			case TextDiffLine::Del:
				if (pane == Pane::Left) {
					item.line_number = linenum++;
				}
				break;
			default:
				item.line_number = linenum; // 行番号は設定するが、インクリメントはしない
				break;
			}

			out->push_back(item);
		}
	};
	SetLineNumber(left, Pane::Left, &m->left_lines);
	SetLineNumber(right, Pane::Right, &m->right_lines);

	ui->widget_diff_left->setText(&m->left_lines, m->mainwindow, diff.blob.a_id, diff.path);
	ui->widget_diff_right->setText(&m->right_lines, m->mainwindow, diff.blob.b_id, diff.path);
	refrectScrollBar();
	ui->widget_diff_slider->clear(true);
}

FileViewType FileDiffWidget::setupPreviewWidget()
{
	clearDiffView();

	QString mimetype_l = m->mainwindow->determinFileType(m->init_param_.bytes_a, true);
	QString mimetype_r = m->mainwindow->determinFileType(m->init_param_.bytes_b, true);

	if (misc::isImage(mimetype_l) || misc::isImage(mimetype_r)) { // image

		ui->verticalScrollBar->setVisible(false);
		ui->horizontalScrollBar->setVisible(false);
		ui->widget_diff_slider->setVisible(false);

		ui->widget_diff_left->setImage(mimetype_l, m->init_param_.bytes_a, m->init_param_.diff.blob.a_id, m->init_param_.diff.path);
		ui->widget_diff_right->setImage(mimetype_r, m->init_param_.bytes_b, m->init_param_.diff.blob.b_id, m->init_param_.diff.path);

		return FileViewType::Image;

	} else { // text

		ui->verticalScrollBar->setVisible(true);
		ui->horizontalScrollBar->setVisible(true);
		ui->widget_diff_slider->setVisible(true);

		setViewType(FileViewType::Text);
		return FileViewType::Text;
	}
}

void FileDiffWidget::setSingleFile(QByteArray const &ba, QString const &id, QString const &path)
{
	m->init_param_ = InitParam_();
	m->init_param_.view_style = FileDiffWidget::ViewStyle::SingleFile;
	m->init_param_.bytes_a = ba;
	m->init_param_.diff.path = path;
	m->init_param_.diff.blob.a_id = id;
}

void FileDiffWidget::setOriginalLines_(QByteArray const &ba)
{
	m->original_lines.clear();
	if (!ba.isEmpty()) {
		char const *begin = ba.data();
		char const *end = begin + ba.size();
		misc::splitLines(begin, end, &m->original_lines, true);
	}
}

void FileDiffWidget::setLeftOnly(QByteArray const &ba, Git::Diff const &diff)
{
	m->init_param_ = InitParam_();
	m->init_param_.view_style = FileDiffWidget::ViewStyle::LeftOnly;
	m->init_param_.bytes_a = ba;
	m->init_param_.diff = diff;

	setOriginalLines_(ba);

	if (setupPreviewWidget() == FileViewType::Text) {

		TextDiffLineList left_lines;
		TextDiffLineList right_lines;

		for (std::string const &line : m->original_lines) {
			left_lines.push_back(TextDiffLine(line, TextDiffLine::Del));
			right_lines.push_back(TextDiffLine());
		}

		setDiffText(diff, left_lines, right_lines);
	}
}

void FileDiffWidget::setRightOnly(QByteArray const &ba, Git::Diff const &diff)
{
	m->init_param_ = InitParam_();
	m->init_param_.view_style = FileDiffWidget::ViewStyle::RightOnly;
	m->init_param_.bytes_b = ba;
	m->init_param_.diff = diff;

	setOriginalLines_(ba);

	if (setupPreviewWidget() == FileViewType::Text) {

		TextDiffLineList left_lines;
		TextDiffLineList right_lines;

		for (std::string const &line : m->original_lines) {
			left_lines.push_back(TextDiffLine());
			right_lines.push_back(TextDiffLine(line, TextDiffLine::Add));
		}

		setDiffText(diff, left_lines, right_lines);
	}
}

void FileDiffWidget::setSideBySide(QByteArray const &ba, Git::Diff const &diff, bool uncommited, QString const &workingdir)
{
	m->init_param_ = InitParam_();
	m->init_param_.view_style = FileDiffWidget::ViewStyle::SideBySideText;
	m->init_param_.bytes_a = ba;
	m->init_param_.diff = diff;
	m->init_param_.uncommited = uncommited;
	m->init_param_.workingdir = workingdir;

	setOriginalLines_(ba);

	if (setupPreviewWidget() == FileViewType::Text) {

		TextDiffLineList left_lines;
		TextDiffLineList right_lines;

		makeSideBySideDiffData(diff, m->original_lines, &left_lines, &right_lines);

		setDiffText(diff, left_lines, right_lines);
	}
}

void FileDiffWidget::setSideBySide_(QByteArray const &ba_a, QByteArray const &ba_b, QString const &workingdir)
{
	m->init_param_ = InitParam_();
	m->init_param_.view_style = FileDiffWidget::ViewStyle::SideBySideImage;
	m->init_param_.bytes_a = ba_a;
	m->init_param_.bytes_b = ba_b;
	m->init_param_.workingdir = workingdir;

	setOriginalLines_(ba_a);

	if (setupPreviewWidget() == FileViewType::Text) {

		TextDiffLineList left_lines;
		TextDiffLineList right_lines;

		makeSideBySideDiffData(m->init_param_.diff, m->original_lines, &left_lines, &right_lines);

		setDiffText(m->init_param_.diff, left_lines, right_lines);
	}
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

	if (isValidID_(info.blob.a_id) && isValidID_(info.blob.b_id)) {
		Git::Object obj_a = cat_file(g, info.blob.a_id);
		Git::Object obj_b = cat_file(g, info.blob.b_id);
		QString mime_a = m->mainwindow->determinFileType(obj_a.content, true);
		QString mime_b = m->mainwindow->determinFileType(obj_b.content, true);
		if (misc::isImage(mime_a) && misc::isImage(mime_b)) {
			setSideBySide_(obj_a.content, obj_b.content, g->workingRepositoryDir());
			return;
		}
	}

	{
		Git::Diff diff;
		if (isValidID_(info.blob.a_id) && isValidID_(info.blob.b_id)) {
			std::string text = GitDiff::diffFile(g, info.blob.a_id, info.blob.b_id).toStdString();
			GitDiff::parseDiff(text, &info, &diff);
		} else {
			diff = info;
		}

		Git::Object obj;
		if (isValidID_(diff.blob.a_id)) { // 左が有効
			obj = cat_file(g, diff.blob.a_id);
			if (isValidID_(diff.blob.b_id)) { // 右が有効
				setSideBySide(obj.content, diff, uncommited, g->workingRepositoryDir()); // 通常のdiff表示
			} else {
				setLeftOnly(obj.content, diff); // 右が無効の時は、削除されたファイル
			}
		} else if (isValidID_(diff.blob.b_id)) { // 左が無効で右が有効の時は、追加されたファイル
			obj = cat_file(g, diff.blob.b_id);
			setRightOnly(obj.content, diff);
		}
	}
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
	std::string text = GitDiff::diffFile(g, diff.blob.a_id, diff.blob.b_id).toStdString();
	GitDiff::parseDiff(text, &diff, &diff);

	Git::Object obj = cat_file(g, diff.blob.a_id);
	setSideBySide(obj.content, diff, false, g->workingRepositoryDir());

	ui->widget_diff_slider->clear(false);

	resetScrollBarValue();
	updateControls();

	ui->widget_diff_slider->update();
}

void FileDiffWidget::resizeEvent(QResizeEvent *)
{
	refrectScrollBar();
}

void FileDiffWidget::keyPressEvent(QKeyEvent *event)
{
	if (event->key() == Qt::Key_Escape) {
		emit escPressed();
		return;
	}

	if (focusWidget() == ui->widget_diff_left->texteditor()) {
		ui->widget_diff_left->write(event);
	} else if (focusWidget() == ui->widget_diff_right->texteditor()) {
		ui->widget_diff_right->write(event);
	}
}

void FileDiffWidget::scrollTo(int value)
{
	ui->verticalScrollBar->setValue(value);
}

void FileDiffWidget::onVerticalScrollValueChanged(int)
{
	refrectScrollBar();
}

void FileDiffWidget::onHorizontalScrollValueChanged(int)
{
	refrectScrollBar();
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
	auto MakePixmap = [&](TextDiffLineList const &lines, int w, int h){
		const int scale = 1;
		QPixmap pixmap = QPixmap(w, h * scale);
		pixmap.fill(Qt::white);
		QPainter pr(&pixmap);
		auto Loop = [&](std::function<QColor(TextDiffLine::Type)> getcolor){
			int i = 0;
			while (i < lines.size()) {
				TextDiffLine::Type type = (TextDiffLine::Type)lines[i].type;
				int j = i + 1;
				if (type != TextDiffLine::Normal) {
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
	if (side == ViewType::Left)  return MakePixmap(diffdata->left->lines, width, height);
	if (side == ViewType::Right) return MakePixmap(diffdata->right->lines, width, height);
	return QPixmap();
}

void FileDiffWidget::on_toolButton_fullscreen_clicked()
{
	BigDiffWindow win(m->mainwindow);
	win.setWindowState(Qt::WindowMaximized);
	win.init(m->mainwindow, m->init_param_);
	win.exec();
}

void FileDiffWidget::setFocusAcceptable(bool f)
{
	Qt::FocusPolicy focuspolicy = f ? Qt::StrongFocus : Qt::NoFocus;
	ui->widget_diff_left->setFocusPolicy(focuspolicy);
	ui->widget_diff_right->setFocusPolicy(focuspolicy);
}

void FileDiffWidget::onUpdateSliderBar()
{
#if 0
	int page = ui->widget_diff_left->height();
	page /= ui->widget_diff_left->lineHeight();
	ui->widget_diff_slider->setScrollPos(ui->verticalScrollBar->maximum(), ui->verticalScrollBar->value(), page);
#else
	int total = m->engine_left->document.lines.size();
	int value = ui->verticalScrollBar->value();
	int page = ui->verticalScrollBar->pageStep();
	ui->widget_diff_slider->setScrollPos(total, value, page);
#endif
}

void FileDiffWidget::refrectScrollBar()
{
	ui->widget_diff_left->refrectScrollBar();
	ui->widget_diff_right->refrectScrollBar();
	onUpdateSliderBar();
}


QPixmap FileDiffWidget::makeDiffPixmap(Pane pane, int width, int height)
{
	auto MakePixmap = [&](TextDiffLineList const &lines, int w, int h){
		const int scale = 1;
		QPixmap pixmap = QPixmap(w, h * scale);
		pixmap.fill(Qt::white);
		QPainter pr(&pixmap);
		auto Loop = [&](std::function<QColor(TextDiffLine::Type)> getcolor){
			int i = 0;
			while (i < lines.size()) {
				TextDiffLine::Type type = (TextDiffLine::Type)lines[i].type;
				int j = i + 1;
				if (type != TextDiffLine::Normal) {
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
			case TextDiffLine::Unknown: return ui->widget_diff_left->theme()->bgDiffUnknown();
			}
			return QColor();
		});
		Loop([&](TextDiffLine::Type t)->QColor{
			switch (t) {
			case TextDiffLine::Add: return ui->widget_diff_left->theme()->bgDiffAddDark();
			case TextDiffLine::Del: return ui->widget_diff_left->theme()->bgDiffDelDark();
			}
			return QColor();
		});
		if (scale == 1) return pixmap;
		return pixmap.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	};
	if (pane == Pane::Left)  return MakePixmap(m->left_lines, width, height);
	if (pane == Pane::Right) return MakePixmap(m->right_lines, width, height);
	return QPixmap();
}


void FileDiffWidget::onMoved(int cur_row, int cur_col, int scr_row, int scr_col)
{
	(void)cur_col;
	(void)cur_row;
	ui->widget_diff_left->move(-1, -1, scr_row, scr_col, false);
	ui->widget_diff_right->move(-1, -1, scr_row, scr_col, false);
	refrectScrollBar();
	onUpdateSliderBar();
}

