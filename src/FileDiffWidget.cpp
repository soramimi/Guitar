#include "FileDiffWidget.h"
#include "ui_FileDiffWidget.h"

#include "BigDiffWindow.h"
#include "GitDiff.h"
#include "joinpath.h"
#include "MainWindow.h"
#include "misc.h"
#include "TypeTraits.h"

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

	setFocusAcceptable(true);

	int n = style()->pixelMetric(QStyle::PM_ScrollBarExtent);
	ui->toolButton_fullscreen->setFixedSize(n, n);

	setMaximizeButtonEnabled(true);

	ui->widget_diff_left->installEventFilter(this);
	ui->widget_diff_right->installEventFilter(this);
	ui->widget_diff_pixmap->installEventFilter(this);
}

FileDiffWidget::~FileDiffWidget()
{
	delete m;
	delete ui;
}

void FileDiffWidget::setLeftBorderVisible(bool f)
{
	ui->widget_diff_left->setLeftBorderVisible(f);
}

void FileDiffWidget::setMaximizeButtonEnabled(bool f)
{
	ui->toolButton_fullscreen->setVisible(f);
	ui->toolButton_fullscreen->setEnabled(f);
}

FileDiffWidget::DiffData *FileDiffWidget::diffdata()
{
	return &m->diff_data;
}

const FileDiffWidget::DiffData *FileDiffWidget::diffdata() const
{
	return &m->diff_data;
}

FileDiffWidget::DrawData *FileDiffWidget::drawdata()
{
	return &m->draw_data;
}

const FileDiffWidget::DrawData *FileDiffWidget::drawdata() const
{
	return &m->draw_data;
}

FileDiffWidget::ViewStyle FileDiffWidget::viewstyle() const
{
	return m->init_param_.view_style;
}

void FileDiffWidget::bindContent_()
{
	ui->widget_diff_pixmap->bind(m->mainwindow, this, diffdata(), drawdata());
	ui->widget_diff_left->bind(m->mainwindow, diffdata()->left, drawdata());
	ui->widget_diff_right->bind(m->mainwindow, diffdata()->right, drawdata());
}

void FileDiffWidget::bind(MainWindow *mw)
{
	Q_ASSERT(mw);
	m->mainwindow = mw;
	bindContent_();

	connect(ui->verticalScrollBar, SIGNAL(valueChanged(int)), this, SLOT(onVerticalScrollValueChanged(int)));
	connect(ui->horizontalScrollBar, SIGNAL(valueChanged(int)), this, SLOT(onHorizontalScrollValueChanged(int)));
	connect(ui->widget_diff_pixmap, SIGNAL(scrollByWheel(int)), this, SLOT(onDiffWidgetWheelScroll(int)));
	connect(ui->widget_diff_pixmap, SIGNAL(valueChanged(int)), this, SLOT(onScrollValueChanged2(int)));
	connect(ui->widget_diff_left, SIGNAL(scrollByWheel(int)), this, SLOT(onDiffWidgetWheelScroll(int)));
	connect(ui->widget_diff_left, SIGNAL(resized()), this, SLOT(onDiffWidgetResized()));
	connect(ui->widget_diff_right, SIGNAL(scrollByWheel(int)), this, SLOT(onDiffWidgetWheelScroll(int)));
	connect(ui->widget_diff_right, SIGNAL(resized()), this, SLOT(onDiffWidgetResized()));
	connect(ui->widget_diff_left, SIGNAL(onBinaryMode()), this, SLOT(setBinaryMode()));
	connect(ui->widget_diff_right, SIGNAL(onBinaryMode()), this, SLOT(setBinaryMode()));
}

GitPtr FileDiffWidget::git()
{
	return m->mainwindow->git();
}

Git::Object FileDiffWidget::cat_file(GitPtr /*g*/, QString const &id)
{
	return m->mainwindow->cat_file(id);
}

void FileDiffWidget::clearDiffView()
{
	diffdata()->clear();
	ui->widget_diff_pixmap->clear(false);
	ui->widget_diff_left->clear();
	ui->widget_diff_right->clear();
	bindContent_();
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
		if (lines_per_widget < diffdata()->left->lines.size() + 1) {
			sb->setRange(0, diffdata()->left->lines.size() - lines_per_widget + 1);
			sb->setPageStep(lines_per_widget);
			return;
		}
	}
	sb->setRange(0, 0);
	sb->setPageStep(0);
}

void FileDiffWidget::updateHorizontalScrollBar()
{
	int headerchars = 0;
	if (viewstyle() == Terminal) {
		headerchars = 0;
	} else if (viewstyle() == SideBySide) {
		headerchars = 10;
	} else {
		headerchars = 5;
	}

	QScrollBar *sb = ui->horizontalScrollBar;
	if (drawdata()->char_width) {
		int chars = ui->widget_diff_left->width() / drawdata()->char_width;
		sb->setRange(0, m->max_line_length + headerchars - chars);
		sb->setPageStep(chars);
		return;
	}
	sb->setRange(0, 0);
	sb->setPageStep(0);
}

void FileDiffWidget::scrollToBottom()
{
	QScrollBar *sb = ui->verticalScrollBar;
	sb->setValue(sb->maximum());
}

void FileDiffWidget::updateSliderCursor()
{
	if (viewstyle() == SideBySide) {
		int total = totalTextLines();
		int value = fileviewScrollPos();
		int size = visibleLines();
		ui->widget_diff_pixmap->setScrollPos(total, value, size);
	}
}

void FileDiffWidget::updateControls()
{
	updateVerticalScrollBar();
	updateHorizontalScrollBar();
	updateSliderCursor();
}



void FileDiffWidget::makeSideBySideDiffData(QList<TextDiffLine> *left_lines, QList<TextDiffLine> *right_lines) const
{
	Git::Diff const &diff = m->init_param_.diff;

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
				QString line = *it;
				ushort c = line.utf16()[0];
				line = line.mid(1);
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
			for (auto it = tmp_left.rbegin(); it != tmp_left.rend(); it++) left_lines->push_back(*it);
			for (auto it = tmp_right.rbegin(); it != tmp_right.rend(); it++) right_lines->push_back(*it);
			linenum = hi.pos;
			h--;
		}
		if (linenum > 0) {
			linenum--;
			if (linenum < (size_t)diffdata()->original_lines.size()) {
				QString line = diffdata()->original_lines[linenum];
				left_lines->push_back(TextDiffLine(line, TextDiffLine::Normal));
				right_lines->push_back(TextDiffLine(line, TextDiffLine::Normal));
			}
		}
	}

	std::reverse(left_lines->begin(), left_lines->end());
	std::reverse(right_lines->begin(), right_lines->end());
}

void FileDiffWidget::setDiffText(QList<TextDiffLine> const &left, QList<TextDiffLine> const &right)
{
	QPixmap pm(1, 1);
	QPainter pr(&pm);
	pr.setFont(ui->widget_diff_left->font());
	QFontMetrics fm = pr.fontMetrics();

	m->max_line_length = 0;

	enum Pane {
		Left,
		Right,
	};
	auto MakeDiffLines = [&](QList<TextDiffLine> const &lines, Pane pane, QList<TextDiffLine> *out){
		out->clear();
		int linenum = 0;
		for (TextDiffLine const &line : lines) {
			TextDiffLine item = line;

			switch (item.type) {
			case TextDiffLine::Normal:
				item.line_number = linenum++;
				break;
			case TextDiffLine::Add:
				if (pane == Right) {
					item.line_number = linenum++;
				}
				break;
			case TextDiffLine::Del:
				if (pane == Left) {
					item.line_number = linenum++;
				}
				break;
			}

			out->push_back(item);

			QString text = FilePreviewWidget::formatText(item.text);
			QSize sz = fm.size(0, text);
			if (m->max_line_length < sz.width()) {
				m->max_line_length = sz.width();
			}
		}
	};
	MakeDiffLines(left, Left, &diffdata()->left->lines);
	MakeDiffLines(right, Right, &diffdata()->right->lines);

	if (drawdata()->char_width > 0) {
		m->max_line_length /= drawdata()->char_width;
	} else {
		m->max_line_length = 0;
	}

	ui->widget_diff_left->update();
	ui->widget_diff_right->update();

	resetScrollBarValue();
	updateControls();
}

FilePreviewType FileDiffWidget::setupPreviewWidget()
{
	clearDiffView();

	// object data

	{
		Git::Diff const &diff = m->init_param_.diff;
		diffdata()->left->path = diff.path;
		diffdata()->right->path = diff.path;
		diffdata()->left->id = diff.blob.a_id;
		diffdata()->right->id = diff.blob.b_id;
	}

	// layout widgets

	{
		// 水平スクロールバーを取り外す
		int i = ui->gridLayout->indexOf(ui->horizontalScrollBar);
		if (i >= 0) delete ui->gridLayout->takeAt(i); // this will deleted QWidgetItem, it wrapped horizontalScrollBar.

		ViewStyle vs = m->init_param_.view_style;
		bool single = vs == ViewStyle::Terminal || vs == ViewStyle::SingleFile;

		if (single) { // 1ファイル表示モード
			ui->gridLayout->addWidget(ui->horizontalScrollBar, 1, 1, 1, 1); // 水平スクロールバーを colspan=1 で据え付ける
			ui->widget_diff_right->setVisible(false);  // 右ファイルビューを非表示
			ui->widget_diff_pixmap->setVisible(false); // pixmap非表示
		} else { // 2ファイル表示モード
			ui->gridLayout->addWidget(ui->horizontalScrollBar, 1, 1, 1, 2); // 水平スクロールバーを colspan=2 で据え付ける
			ui->widget_diff_right->setVisible(true);  // 右ファイルビューを表示
			ui->widget_diff_pixmap->setVisible(true); // pixmap表示
		}
	}

	// init content

	diffdata()->left->bytes = m->init_param_.bytes;

	QString mimetype = m->mainwindow->determinFileType(diffdata()->left->bytes, true);

	ui->widget_diff_left->setFileType(mimetype);
	ui->widget_diff_right->setFileType(mimetype);

	if (misc::isImageFile(mimetype)) { // image

		ui->verticalScrollBar->setVisible(false);
		ui->horizontalScrollBar->setVisible(false);
		QPixmap pixmap;
		pixmap.loadFromData(diffdata()->left->bytes);

		FilePreviewWidget *w = ui->widget_diff_left;
		if (m->init_param_.view_style == FileDiffWidget::ViewStyle::RightOnly) {
			w = ui->widget_diff_right;
		}

		w->setImage(mimetype, pixmap);

		return w->filetype();

	} else { // text

		ui->verticalScrollBar->setVisible(true);
		ui->horizontalScrollBar->setVisible(true);

		if (diffdata()->left->bytes.isEmpty()) {
			diffdata()->original_lines.clear();
		} else {
			diffdata()->original_lines = misc::splitLines(diffdata()->left->bytes, [](char const *ptr, size_t len){ return QString::fromUtf8(ptr, len); });
		}

		return FilePreviewType::Text;
	}
}

void FileDiffWidget::setBinaryMode(bool f)
{
	diffdata()->original_lines.clear();
	ui->widget_diff_left->setBinaryMode(f);
	ui->widget_diff_right->setBinaryMode(f);
	ui->widget_diff_left->update();
	ui->widget_diff_right->update();
}

void FileDiffWidget::setBinaryMode()
{
	setBinaryMode(true);
}

void FileDiffWidget::setSingleFile(QByteArray const &ba, QString const &id, QString const &path)
{
	m->init_param_ = InitParam_();
	m->init_param_.view_style = FileDiffWidget::ViewStyle::SingleFile;
	m->init_param_.bytes = ba;
	m->init_param_.diff.path = path;
	m->init_param_.diff.blob.a_id = id;

	if (setupPreviewWidget() == FilePreviewType::Text) {

		QList<TextDiffLine> left_lines;
		QList<TextDiffLine> right_lines;

		for (QString const &line : diffdata()->original_lines) {
			left_lines.push_back(TextDiffLine(line, TextDiffLine::Normal));
		}

		setDiffText(left_lines, right_lines);
	}
}

void FileDiffWidget::setLeftOnly(QByteArray const &ba, Git::Diff const &diff)
{
	m->init_param_ = InitParam_();
	m->init_param_.view_style = FileDiffWidget::ViewStyle::LeftOnly;
	m->init_param_.bytes = ba;
	m->init_param_.diff = diff;

	if (setupPreviewWidget() == FilePreviewType::Text) {

		QList<TextDiffLine> left_lines;
		QList<TextDiffLine> right_lines;

		for (QString const &line : diffdata()->original_lines) {
			left_lines.push_back(TextDiffLine(line, TextDiffLine::Del));
			right_lines.push_back(TextDiffLine());
		}

		setDiffText(left_lines, right_lines);
	}
}

void FileDiffWidget::setRightOnly(QByteArray const &ba, Git::Diff const &diff)
{
	m->init_param_ = InitParam_();
	m->init_param_.view_style = FileDiffWidget::ViewStyle::RightOnly;
	m->init_param_.bytes = ba;
	m->init_param_.diff = diff;

	if (setupPreviewWidget() == FilePreviewType::Text) {

		QList<TextDiffLine> left_lines;
		QList<TextDiffLine> right_lines;

		for (QString const &line : diffdata()->original_lines) {
			left_lines.push_back(TextDiffLine());
			right_lines.push_back(TextDiffLine(line, TextDiffLine::Add));
		}

		setDiffText(left_lines, right_lines);
	}
}

void FileDiffWidget::setSideBySide(QByteArray const &ba, Git::Diff const &diff, bool uncommited, QString const &workingdir)
{
	m->init_param_ = InitParam_();
	m->init_param_.view_style = FileDiffWidget::ViewStyle::SideBySide;
	m->init_param_.bytes = ba;
	m->init_param_.diff = diff;
	m->init_param_.uncommited = uncommited;
	m->init_param_.workingdir = workingdir;

	if (setupPreviewWidget() == FilePreviewType::Text) {

		if (uncommited) {
			QString path = workingdir / diff.path;
			diffdata()->right->id = GitDiff::prependPathPrefix(path);
		}

		QList<TextDiffLine> left_lines;
		QList<TextDiffLine> right_lines;

		makeSideBySideDiffData(&left_lines, &right_lines);

		setDiffText(left_lines, right_lines);
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

	Git::Diff diff;
	if (isValidID_(info.blob.a_id) && isValidID_(info.blob.b_id)) {
		QString text = GitDiff::diffFile(g, info.blob.a_id, info.blob.b_id);
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

	Git::Object obj = cat_file(g, diff.blob.a_id);
	setSideBySide(obj.content, diff, false, g->workingRepositoryDir());

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
			if (e->modifiers() & Qt::AltModifier) {
				switch (k) {
				case Qt::Key_Up:
					emit movePreviousItem();
					return true;
				case Qt::Key_Down:
					emit moveNextItem();
					return true;
				}
			} else {
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
	}
	return false;
}

void FileDiffWidget::scrollTo(int value)
{
	drawdata()->v_scroll_pos = value;
	ui->widget_diff_left->update();
	ui->widget_diff_right->update();
}

void FileDiffWidget::onVerticalScrollValueChanged(int value)
{
	scrollTo(value);
	updateSliderCursor();
}

void FileDiffWidget::onHorizontalScrollValueChanged(int value)
{
	m->draw_data.h_scroll_pos = value;
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

namespace{
	class MakePixmapLoop {
	private:
		std::reference_wrapper<const QList<TextDiffLine>> lines_;
		int w_;
		int pixmap_height_;
		std::reference_wrapper<QPainter> pr_;
	public:
		MakePixmapLoop() = delete;
		MakePixmapLoop(const MakePixmapLoop&) = delete;
		MakePixmapLoop(MakePixmapLoop&&) = delete;
		MakePixmapLoop& operator=(const MakePixmapLoop&) = delete;
		MakePixmapLoop& operator=(MakePixmapLoop&&) = delete;
		MakePixmapLoop(QList<TextDiffLine> const &lines, int w, int pixmap_height, QPainter& pr)
			: lines_(lines), w_(w), pixmap_height_(pixmap_height), pr_(pr)
		{}
		template<
			typename GetColorFunc,
			typename std::enable_if<
				type_traits::is_callable<GetColorFunc(TextDiffLine::Type), QColor>::value,
				std::nullptr_t
			>::type = nullptr
		>
		void operator()(GetColorFunc&& getcolor)
		{
			int i = 0;
			while (i < this->lines_.get().size()) {
				TextDiffLine::Type type = this->lines_.get()[i].type;
				int j = i + 1;
				if (type != TextDiffLine::Normal) {
					while (j < this->lines_.get().size()) {
						if (this->lines_.get()[j].type != type) break;
						j++;
					}
					int y = i * this->pixmap_height_ / this->lines_.get().size();
					int z = j * this->pixmap_height_ / this->lines_.get().size();
					if (z == y) z = y + 1;
					QColor color = getcolor(type);
					if (color.isValid()) this->pr_.get().fillRect(0, y, this->w_, z - y, color);
				}
				i = j;
			}
		}
	};
}
QPixmap FileDiffWidget::makeDiffPixmap(ViewType side, int width, int height, FileDiffWidget::DiffData const *diffdata, FileDiffWidget::DrawData const *drawdata)
{
	auto MakePixmap = [&](QList<TextDiffLine> const &lines, int w, int h){
		const int scale = 1;
		QPixmap pixmap = QPixmap(w, h * scale);
		pixmap.fill(Qt::white);
		QPainter pr(&pixmap);
		MakePixmapLoop Loop(lines, w, pixmap.height(), pr);
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

void FileDiffWidget::setTerminalMode()
{
	setSingleFile(QByteArray(), QString(), QString());
	m->init_param_.view_style = ViewStyle::Terminal;
	ui->widget_diff_left->setTerminalMode(true);
	ui->horizontalScrollBar->setVisible(false);
}

bool FileDiffWidget::isTerminalMode() const
{
	return m->init_param_.view_style == ViewStyle::Terminal;
}

void FileDiffWidget::termWrite(ushort c)
{
	if (!isTerminalMode()) return;

	if (c == '\r') {
		m->term_cursor_col = 0;
		return;
	}
	if (c == '\n') {
		m->term_cursor_col = 0;
		m->term_cursor_row++;
		return;
	}

	QList<TextDiffLine> *lines = &m->diff_data.left->lines;

	while (lines->size() <= m->term_cursor_row) {
		lines->push_back(TextDiffLine(TextDiffLine::Normal, 100));
	}

	TextDiffLine *line = &(*lines)[m->term_cursor_row];
	while ((int)line->text.size() <= m->term_cursor_col) {
		line->text.push_back(' ');
	}

	line->text[m->term_cursor_col] = c;
	m->term_cursor_col++;
}

void FileDiffWidget::termWrite(ushort const *begin, ushort const *end)
{
	ushort const *p = begin;
	while (p < end) {
		termWrite(*p);
		p++;
	}
}

void FileDiffWidget::termWrite(QString const &text)
{
	ushort const *begin = text.utf16();
	ushort const *end = begin + text.size();
	termWrite(begin, end);
}

void FileDiffWidget::setFocusAcceptable(bool f)
{
	Qt::FocusPolicy focuspolicy = f ? Qt::StrongFocus : Qt::NoFocus;
	ui->widget_diff_left->setFocusPolicy(focuspolicy);
	ui->widget_diff_right->setFocusPolicy(focuspolicy);
}



