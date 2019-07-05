
#include "ApplicationGlobal.h"
#include "BigDiffWindow.h"
#include "FileDiffWidget.h"
#include "GitDiff.h"
#include "MainWindow.h"
#include "Theme.h"
#include "common/joinpath.h"
#include "common/misc.h"
#include "ui_FileDiffWidget.h"
#include <QBuffer>
#include <QDebug>
#include <QKeyEvent>
#include <QMenu>
#include <QPainter>
#include <QStyle>
#include <QTextCodec>
#include <memory>

enum {
	DiffIndexRole = Qt::UserRole,
};

struct FileDiffWidget::Private {
	BasicMainWindow *mainwindow = nullptr;
	FileDiffWidget::InitParam_ init_param_;
	Git::CommitItemList commit_item_list;
	std::vector<std::string> original_lines;
	TextEditorEnginePtr engine_left;
	TextEditorEnginePtr engine_right;
	TextDiffLineList left_lines;
	TextDiffLineList right_lines;
	int max_line_length = 0;

	int term_cursor_row = 0;
	int term_cursor_col = 0;

	QTextCodec *text_codec = nullptr;
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

	ui->widget_diff_slider->init([&](DiffPane pane, int width, int height){
		return makeDiffPixmap(pane, width, height);
	}, global->theme);

	connect(ui->widget_diff_slider, &FileDiffSliderWidget::valueChanged, this, &FileDiffWidget::scrollTo);
	connect(ui->widget_diff_left->texteditor(), &TextEditorWidget::moved, this, &FileDiffWidget::onMoved);
	connect(ui->widget_diff_right->texteditor(), &TextEditorWidget::moved, this, &FileDiffWidget::onMoved);

	setFocusAcceptable(Qt::ClickFocus);
	QWidget::setTabOrder(ui->widget_diff_slider, ui->widget_diff_left);
	QWidget::setTabOrder(ui->widget_diff_left, ui->widget_diff_right);

	int n = style()->pixelMetric(QStyle::PM_ScrollBarExtent);
	ui->toolButton_fullscreen->setFixedSize(n, n);

	setMaximizeButtonEnabled(true);

	m->engine_left = std::make_shared<TextEditorEngine>();
	m->engine_right = std::make_shared<TextEditorEngine>();
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



void FileDiffWidget::bind(BasicMainWindow *mw)
{
	Q_ASSERT(mw);
	m->mainwindow = mw;
	ui->widget_diff_left->bind(mw, this, ui->verticalScrollBar, ui->horizontalScrollBar, mw->themeForTextEditor());
	ui->widget_diff_right->bind(mw, this, ui->verticalScrollBar, ui->horizontalScrollBar, mw->themeForTextEditor());

	connect(ui->verticalScrollBar, &QAbstractSlider::valueChanged, this, &FileDiffWidget::onVerticalScrollValueChanged);
	connect(ui->horizontalScrollBar, &QAbstractSlider::valueChanged, this, &FileDiffWidget::onHorizontalScrollValueChanged);
}

BasicMainWindow *FileDiffWidget::mainwindow()
{
	return m->mainwindow;
}

GitPtr FileDiffWidget::git()
{
	if (!mainwindow()) {
		qDebug() << "Maybe, you forgot to call FileDiffWidget::bind() ?";
		return GitPtr();
	}
	return mainwindow()->git();
}

Git::Object FileDiffWidget::cat_file(GitPtr const &/*g*/, QString const &id)
{
	return mainwindow()->cat_file(id);
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
					tmp_left.emplace_back();
					minus++;
				}
				while (minus > plus) {
					tmp_right.emplace_back();
					plus++;
				}
				minus = plus = 0;
			};
			for (auto line : hi.lines) {
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
			for (auto it = tmp_left.rbegin(); it != tmp_left.rend(); it++) {
				TextDiffLine l(*it);
				if (m->text_codec) {
					if (!l.text.isEmpty()) {
						QString s = QString::fromUtf8(l.text.data(), l.text.size());
						l.text = m->text_codec->fromUnicode(s);
					}
				}
				left_lines->push_back(l);
			}
			for (auto it = tmp_right.rbegin(); it != tmp_right.rend(); it++) {
				TextDiffLine l(*it);
				if (m->text_codec) {
					if (!l.text.isEmpty()) {
						QString s = QString::fromUtf8(l.text.data(), l.text.size());
						l.text = m->text_codec->fromUnicode(s);
					}
				}
				right_lines->push_back(l);
			}
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

	ui->widget_diff_left->setText(&m->left_lines, mainwindow(), diff.blob.a_id, diff.path);
	ui->widget_diff_right->setText(&m->right_lines, mainwindow(), diff.blob.b_id, diff.path);
	refrectScrollBar();
	ui->widget_diff_slider->clear(true);
}

FileViewType FileDiffWidget::setupPreviewWidget()
{
	clearDiffView();

	QString mimetype_l = mainwindow()->determinFileType(m->init_param_.bytes_a, true);
	QString mimetype_r = mainwindow()->determinFileType(m->init_param_.bytes_b, true);

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

QString FileDiffWidget::diffObjects(GitPtr const &g, QString const &a_id, QString const &b_id)
{
	if (m->text_codec) {
		Git::Object obj_a = mainwindow()->cat_file_(g, a_id);
		Git::Object obj_b = mainwindow()->cat_file_(g, b_id);
		if (obj_b.type == Git::Object::Type::UNKNOWN) {
			obj_b.type = Git::Object::Type::BLOB;
		}
		if (obj_a.type == Git::Object::Type::BLOB && obj_b.type == Git::Object::Type::BLOB) {
			QString path_a = mainwindow()->newTempFilePath();
			QString path_b = mainwindow()->newTempFilePath();
			QFile file_a(path_a);
			QFile file_b(path_b);
			if (file_a.open(QFile::WriteOnly) && file_b.open(QFile::WriteOnly)) {
				file_a.write(m->text_codec->toUnicode(obj_a.content).toUtf8());
				file_b.write(m->text_codec->toUnicode(obj_b.content).toUtf8());
				file_a.close();
				file_b.close();
				QString s = g->diff_file(path_a, path_b);
				file_a.remove();
				file_b.remove();
				return s;
			}
		}
	}
	return GitDiff::diffObjects(g, a_id, b_id);
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
		QString mime_a = mainwindow()->determinFileType(obj_a.content, true);
		QString mime_b = mainwindow()->determinFileType(obj_b.content, true);
		if (misc::isImage(mime_a) && misc::isImage(mime_b)) {
			setSideBySide_(obj_a.content, obj_b.content, g->workingRepositoryDir());
			return;
		}
	}

	{
		Git::Diff diff;
		if (isValidID_(info.blob.a_id) && isValidID_(info.blob.b_id)) {
			std::string text = diffObjects(g, info.blob.a_id, info.blob.b_id).toStdString();
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

void FileDiffWidget::updateDiffView(QString const &id_left, QString const &id_right, QString const &path)
{
	GitPtr g = git();
	if (!g) return;
	if (!g->isValidWorkingCopy()) return;

	Git::Diff diff;
	diff.path = path;
	diff.blob.a_id = id_left;
	diff.blob.b_id = id_right;
	diff.mode = "0";
	std::string text = diffObjects(g, diff.blob.a_id, diff.blob.b_id).toStdString();
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
		event->ignore(); // Escが押されたとき、親ウィジェットに処理させる。（通常、ウィンドウを閉じる、など）
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

void FileDiffWidget::on_toolButton_fullscreen_clicked()
{
	if (m->init_param_.diff.blob.a_id.isEmpty() && m->init_param_.diff.blob.b_id.isEmpty()) {
		return;
	}

	BigDiffWindow win(mainwindow());
	win.setWindowState(Qt::WindowMaximized);
	win.init(mainwindow(), m->init_param_);
	win.exec();
}

void FileDiffWidget::setFocusAcceptable(Qt::FocusPolicy focuspolicy)
{
//	Qt::FocusPolicy focuspolicy = f ? Qt::ClickFocus : Qt::NoFocus;
	ui->widget_diff_left->setFocusPolicy(focuspolicy);
	ui->widget_diff_right->setFocusPolicy(focuspolicy);
}

void FileDiffWidget::onUpdateSliderBar()
{
	int total = m->engine_left->document.lines.size();
	int value = ui->verticalScrollBar->value();
	int page = ui->verticalScrollBar->pageStep();
	ui->widget_diff_slider->setScrollPos(total, value, page);
}

void FileDiffWidget::refrectScrollBar()
{
	ui->widget_diff_left->refrectScrollBar();
	ui->widget_diff_right->refrectScrollBar();
	onUpdateSliderBar();
}

QPixmap FileDiffWidget::makeDiffPixmap(DiffPane pane, int width, int height)
{
	auto Do = [&](TextDiffLineList const &lines){
		return FileDiffSliderWidget::makeDiffPixmap(width, height, lines, global->theme);
	};
	if (pane == DiffPane::Left)  return Do(m->left_lines);
	if (pane == DiffPane::Right) return Do(m->right_lines);
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

void FileDiffWidget::setTextCodec(QTextCodec *codec)
{
	m->text_codec = codec;
	ui->widget_diff_left->setTextCodec(codec);
	ui->widget_diff_right->setTextCodec(codec);
	emit textcodecChanged();
}

void FileDiffWidget::setTextCodec(char const *name)
{
	QTextCodec *codec = name ? QTextCodec::codecForName(name) : nullptr;
	setTextCodec(codec);
}

void FileDiffWidget::on_toolButton_menu_clicked()
{
	QMenu menu;
	QAction *a_utf8 = menu.addAction("UTF-8");
	QAction *a_sjis = menu.addAction("SJIS (CP932)");
	QAction *a_eucjp = menu.addAction("EUC-JP");
	QAction *a_iso2022jp = menu.addAction("JIS (ISO-2022-JP)");
	QAction *a = menu.exec(QCursor::pos() + QPoint(8, -8));
	if (a) {
		if (a == a_utf8) {
			setTextCodec((char const *)nullptr);
			return;
		}
		if (a == a_sjis) {
			setTextCodec("Shift_JIS");
			return;
		}
		if (a == a_eucjp) {
			setTextCodec("EUC-JP");
			return;
		}
		if (a == a_iso2022jp) {
			setTextCodec("ISO-2022-JP");
			return;
		}
	}
}
