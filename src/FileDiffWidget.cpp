
#include "FileDiffWidget.h"
#include "ApplicationGlobal.h"
#include "BigDiffWindow.h"
#include "GitDiff.h"
#include "MainWindow.h"
#include "Theme.h"
#include "common/joinpath.h"
#include "common/misc.h"
#include "dtl/dtl.hpp"
#include "ui_FileDiffWidget.h"
#include <QBuffer>
#include <QDebug>
#include <QFile>
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

	std::vector<std::pair<LineFragment, LineFragment>> linefragmentpair;
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

	// 左端のスライダー
	connect(ui->widget_diff_slider, &FileDiffSliderWidget::valueChanged, this, &FileDiffWidget::scrollTo);

	// 左のテキストビュー
	connect(ui->widget_diff_left->texteditor(), &TextEditorView::moved, this, &FileDiffWidget::onMoved);

	// 右のテキストビュー
	connect(ui->widget_diff_right->texteditor(), &TextEditorView::moved, this, &FileDiffWidget::onMoved);

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

MainWindow *FileDiffWidget::mainwindow()
{
	return global->mainwindow;
}

/**
 * @brief スクロールバーのセットアップ
 * @param mw
 */
void FileDiffWidget::init()
{
	ui->widget_diff_left->bind(this, ui->verticalScrollBar, ui->horizontalScrollBar, mainwindow()->themeForTextEditor());
	ui->widget_diff_right->bind(this, ui->verticalScrollBar, ui->horizontalScrollBar, mainwindow()->themeForTextEditor());

	connect(ui->verticalScrollBar, &QAbstractSlider::valueChanged, this, &FileDiffWidget::onVerticalScrollValueChanged);
	connect(ui->horizontalScrollBar, &QAbstractSlider::valueChanged, this, &FileDiffWidget::onHorizontalScrollValueChanged);
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

GitPtr FileDiffWidget::git()
{
	if (!mainwindow()) {
		qDebug() << "Maybe, you forgot to call FileDiffWidget::bind()?";
		return GitPtr();
	}
	return mainwindow()->git();
}

Git::Object FileDiffWidget::catFile(QString const &id)
{
	return mainwindow()->catFile(id);
}

int FileDiffWidget::totalTextLines() const
{
	return m->engine_left->document.lines.size();
}

void FileDiffWidget::clearDiffView()
{
	m->linefragmentpair.clear();

	ui->widget_diff_left->clear();
	ui->widget_diff_right->clear();
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
					v = v * 10 + size_t(*p - '0');
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
			int hunk_number = int(h - 1);
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
                auto const &line = original_lines[linenum];
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
	ASSERT_MAIN_THREAD();
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

	std::vector<LineFragment> left_dels;
	std::vector<LineFragment> right_adds;

	auto Do = [](TextDiffLineList const &lines, Document::Line::Type type, std::vector<LineFragment> *blocks){
		int i = 0;
		while (i < lines.size()) {
			int n = 1;
			while (i + n < lines.size()) {
				if (lines[i + n].type != lines[i].type) break;
				n++;
			}
			if (lines[i].type == type) {
				blocks->emplace_back(type, i, n);
			}
			i += n;
		}
	};
	Do(m->left_lines, Document::Line::Del, &left_dels);
	Do(m->right_lines, Document::Line::Add, &right_adds);

	m->linefragmentpair.clear();

	std::size_t ileft = 0;
	std::size_t iright = 0;
	while (ileft < left_dels.size() && iright < right_adds.size()) {
		if (left_dels[ileft].line_index < right_adds[iright].line_index) {
			ileft++;
		} else if (left_dels[ileft].line_index > right_adds[iright].line_index) {
			iright++;
		} else {
			if (left_dels[ileft].line_count == right_adds[iright].line_count) {
				m->linefragmentpair.emplace_back(left_dels[ileft], right_adds[iright]);
			}
			ileft++;
			iright++;
		}
	}

	ui->widget_diff_left->setText(&m->left_lines, diff.blob.a_id_or_path, diff.path);
	ui->widget_diff_right->setText(&m->right_lines, diff.blob.b_id_or_path, diff.path);
	refrectScrollBar();
	ui->widget_diff_slider->clear(true);
}

/**
 * @brief テキストか画像かでビューを切り替える
 * @return
 */
FileViewType FileDiffWidget::setupPreviewWidget()
{
	clearDiffView();

	QString mimetype_l = mainwindow()->determinFileType(m->init_param_.bytes_a);
	QString mimetype_r = mainwindow()->determinFileType(m->init_param_.bytes_b);

	if (misc::isImage(mimetype_l) || misc::isImage(mimetype_r)) { // image

		ui->verticalScrollBar->setVisible(false);
		ui->horizontalScrollBar->setVisible(false);
		ui->widget_diff_slider->setVisible(false);

//		qDebug() << mimetype_l << m->init_param_.diff.blob.a_id_or_path << mimetype_r << m->init_param_.diff.blob.b_id_or_path;
		ui->widget_diff_left->setImage(mimetype_l, m->init_param_.bytes_a, m->init_param_.diff.blob.a_id_or_path, m->init_param_.diff.path);
		ui->widget_diff_right->setImage(mimetype_r, m->init_param_.bytes_b, m->init_param_.diff.blob.b_id_or_path, m->init_param_.diff.path);

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
	m->init_param_.diff.blob.a_id_or_path = id;
}

void FileDiffWidget::setOriginalLines_(QByteArray const &ba, Git::SubmoduleItem const *submodule, Git::CommitItem const *submodule_commit)
{
	(void)submodule;
	(void)submodule_commit;

	m->original_lines.clear();

	if (!ba.isEmpty()) {
		char const *begin = ba.data();
		char const *end = begin + ba.size();
		misc::splitLines(begin, end, &m->original_lines, true);
	}
}

void FileDiffWidget::setLeftOnly(Git::Diff const &diff, QByteArray const &ba)
{
	m->init_param_ = InitParam_();
	m->init_param_.view_style = FileDiffWidget::ViewStyle::LeftOnly;
	m->init_param_.bytes_a = ba;
	m->init_param_.diff = diff;

	setOriginalLines_(ba, &diff.a_submodule.item, &diff.a_submodule.commit);

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

bool FileDiffWidget::setSubmodule(Git::Diff const &diff)
{
	Git::SubmoduleItem const &submod_a = diff.a_submodule.item;
	Git::SubmoduleItem const &submod_b = diff.b_submodule.item;
	Git::CommitItem const &submod_commit_a = diff.a_submodule.commit;
	Git::CommitItem const &submod_commit_b = diff.b_submodule.commit;
	if (submod_a || submod_b) {
		auto Text = [](Git::SubmoduleItem const *submodule, Git::CommitItem const *submodule_commit, TextDiffLineList *out){
			*out = {};
			if (submodule && *submodule) {
				QString text;
				text += "name: " + submodule->name + '\n';
				text += "path: " + submodule->path + '\n';
				text += "url: " + submodule->url + '\n';
				text += "commit: " + submodule->id.toQString() + '\n';
				text += "date: " + misc::makeDateTimeString(submodule_commit->commit_date) + '\n';
				text += "author: " + submodule_commit->author + '\n';
				text += "email: " + submodule_commit->email + '\n';
				text += '\n';
				text += submodule_commit->message;
				for (QString const &line : misc::splitLines(text)) {
					out->push_back(Document::Line(line.toStdString()));
				}
			}
		};
		TextDiffLineList left_lines;
		TextDiffLineList right_lines;
		if (submod_a) {
			Text(&submod_a, &submod_commit_a, &left_lines);
		}
		if (submod_b) {
			Text(&submod_b, &submod_commit_b, &right_lines);
		}
		setDiffText(diff, left_lines, right_lines);
		return true;
	}
	return false;
}

void FileDiffWidget::setRightOnly(Git::Diff const &diff, QByteArray const &ba)
{
	m->init_param_ = InitParam_();
	m->init_param_.view_style = FileDiffWidget::ViewStyle::RightOnly;
	m->init_param_.bytes_b = ba;
	m->init_param_.diff = diff;

	setOriginalLines_(ba, &diff.b_submodule.item, &diff.b_submodule.commit);

	if (setSubmodule(diff)) {
		// ok
	} else if (setupPreviewWidget() == FileViewType::Text) {

		TextDiffLineList left_lines;
		TextDiffLineList right_lines;

		for (std::string const &line : m->original_lines) {
			left_lines.push_back(TextDiffLine());
			right_lines.push_back(TextDiffLine(line, TextDiffLine::Add));
		}

		setDiffText(diff, left_lines, right_lines);
	}
}

void FileDiffWidget::setSideBySide(Git::Diff const &diff, QByteArray const &ba, bool uncommited, QString const &workingdir)
{
	m->init_param_ = InitParam_();
	m->init_param_.view_style = FileDiffWidget::ViewStyle::SideBySideText;
	m->init_param_.bytes_a = ba;
	m->init_param_.diff = diff;
	m->init_param_.uncommited = uncommited;
	m->init_param_.workingdir = workingdir;

	setOriginalLines_(ba, {}, {});

	if (setSubmodule(diff)) {
		// ok
	} else {
		if (setupPreviewWidget() == FileViewType::Text) {

			TextDiffLineList left_lines;
			TextDiffLineList right_lines;

			makeSideBySideDiffData(diff, m->original_lines, &left_lines, &right_lines);

			setDiffText(diff, left_lines, right_lines);
		}
	}
}

void FileDiffWidget::setSideBySide_(Git::Diff const &diff, QByteArray const &ba_a, QByteArray const &ba_b, QString const &workingdir)
{
	m->init_param_ = InitParam_();
	m->init_param_.view_style = FileDiffWidget::ViewStyle::SideBySideImage;
	m->init_param_.bytes_a = ba_a;
	m->init_param_.bytes_b = ba_b;
	m->init_param_.diff = diff;
	m->init_param_.workingdir = workingdir;

	setOriginalLines_(ba_a, {}, {});

	if (setupPreviewWidget() == FileViewType::Text) {

		TextDiffLineList left_lines;
		TextDiffLineList right_lines;

		makeSideBySideDiffData(m->init_param_.diff, m->original_lines, &left_lines, &right_lines);

		setDiffText(m->init_param_.diff, left_lines, right_lines);
	}
}

QString FileDiffWidget::diffObjects(QString const &a_id, QString const &b_id)
{
	if (m->text_codec) {
		Git::Object obj_a = mainwindow()->catFile(a_id);
		Git::Object obj_b = mainwindow()->catFile(b_id);
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
				QString s = git()->diff_file(path_a, path_b);
				file_a.remove();
				file_b.remove();
				return s;
			}
		}
	}
	return GitDiff::diffObjects(git(), a_id, b_id);
}

/**
 * @brief コミットIDの検証
 * @param id
 * @return
 */
bool FileDiffWidget::isValidID_(QString const &id)
{
	if (id.startsWith(PATH_PREFIX)) {
		return true;
	}
	return Git::isValidID(id);
}

/**
 * @brief 差分ビューを更新
 * @param info
 * @param uncommited
 */
void FileDiffWidget::updateDiffView(Git::Diff const &info, bool uncommited)
{
	ASSERT_MAIN_THREAD();
	
	GitPtr g = git();
	if (!g) return;
	if (!g->isValidWorkingCopy()) return;

	if (isValidID_(info.blob.a_id_or_path) && isValidID_(info.blob.b_id_or_path)) {
		Git::Object obj_a = catFile(info.blob.a_id_or_path);
		Git::Object obj_b = catFile(info.blob.b_id_or_path);
		QString mime_a = mainwindow()->determinFileType(obj_a.content);
		QString mime_b = mainwindow()->determinFileType(obj_b.content);
		if (misc::isImage(mime_a) && misc::isImage(mime_b)) {
			setSideBySide_(info, obj_a.content, obj_b.content, g->workingDir());
			return;
		}
	}

	{
		Git::Diff diff;
		if (isValidID_(info.blob.a_id_or_path) && isValidID_(info.blob.b_id_or_path)) {
			std::string text = diffObjects(info.blob.a_id_or_path, info.blob.b_id_or_path).toStdString();
			GitDiff::parseDiff(text, &info, &diff);
		} else {
			diff = info;
		}

		Git::Object obj;
		if (isValidID_(diff.blob.a_id_or_path)) { // 左が有効
			obj = catFile(diff.blob.a_id_or_path);
			if (isValidID_(diff.blob.b_id_or_path)) { // 右が有効
				setSideBySide(diff, obj.content, uncommited, g->workingDir()); // 通常のdiff表示
			} else {
				setLeftOnly(diff, obj.content); // 右が無効の時は、削除されたファイル
			}
		} else if (isValidID_(diff.blob.b_id_or_path)) { // 左が無効で右が有効の時は、追加されたファイル
			obj = catFile(diff.blob.b_id_or_path);
			setRightOnly(diff, obj.content);
		}
	}
}

/**
 * @brief 差分ビューを更新
 * @param id_left
 * @param id_right
 * @param path
 */
void FileDiffWidget::updateDiffView(QString const &id_left, QString const &id_right, QString const &path)
{
	GitPtr g = git();
	if (!g) return;
	if (!g->isValidWorkingCopy()) return;

	Git::Diff diff;
	diff.path = path;
	diff.blob.a_id_or_path = id_left;
	diff.blob.b_id_or_path = id_right;
	diff.mode = "0";
	std::string text = diffObjects(diff.blob.a_id_or_path, diff.blob.b_id_or_path).toStdString();
	GitDiff::parseDiff(text, &diff, &diff);

	Git::Object obj = catFile(diff.blob.a_id_or_path);
	setSideBySide(diff, obj.content, false, g->workingDir());

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

/**
 * @brief 縦スクロールバーが操作された
 */
void FileDiffWidget::onVerticalScrollValueChanged(int)
{
	refrectScrollBarV();
}

/**
 * @brief 横スクロールバーが操作された
 */
void FileDiffWidget::onHorizontalScrollValueChanged(int)
{
	refrectScrollBarH();
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
	if (m->init_param_.diff.blob.a_id_or_path.isEmpty() && m->init_param_.diff.blob.b_id_or_path.isEmpty()) {
		return;
	}

	BigDiffWindow win(mainwindow());
	win.setWindowState(Qt::WindowMaximized);
	win.init(m->init_param_);
	win.exec();
}

void FileDiffWidget::setFocusAcceptable(Qt::FocusPolicy focuspolicy)
{
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

/**
 * @brief スクロールバーの状態を反映
 * @param updateformat
 */
void FileDiffWidget::refrectScrollBar(/*bool updateformat*/)
{
	ui->widget_diff_left->refrectScrollBar();
	ui->widget_diff_right->refrectScrollBar();

	const bool updateformat = true;
	if (updateformat) {

		// 左と右のテキストを取得
		TextEditorView::FormattedLines *llines = ui->widget_diff_left->getTextEditorView()->fetchLines();
		TextEditorView::FormattedLines *rlines = ui->widget_diff_right->getTextEditorView()->fetchLines();

		// サイドバイサイドのとき文字差分を求める
		if (viewstyle() == SideBySideText) {
			using Char = AbstractCharacterBasedApplication::Char;
			using Attr = AbstractCharacterBasedApplication::CharAttr;

			const size_t n = std::min(llines->size(), rlines->size());

			for (std::size_t i = 0; i < n; i++) {
				std::vector<Char> *lchars = llines->chars(i);
				std::vector<Char> *rchars = rlines->chars(i);
				size_t l = 0;
				size_t r = 0;

				// 文字差分を求める
				// dtl (diff template library) https://github.com/cubicdaiya/dtl
				dtl::Diff<uint32_t, std::vector<Char>> d(*lchars, *rchars);
				d.compose();

				auto const &sq = d.getSes().getSequence();
				for (std::pair<uint32_t, dtl::elemInfo> const &t : sq) {
					switch (t.second.type) {
					case dtl::SES_COMMON:
						l++;
						r++;
						break;
					case dtl::SES_DELETE:
						lchars->at(l).attr.flags |= Attr::Underline1;
						l++;
						break;
					case dtl::SES_ADD:
						rchars->at(r).attr.flags |= Attr::Underline2;
						r++;
						break;
					}
				}
			}
		}
	}

	onUpdateSliderBar();
}

/**
 * @brief 縦スクロールバーの状態を反映
 */
void FileDiffWidget::refrectScrollBarV()
{
	refrectScrollBar();
}

/**
 * @brief 横スクロールバーの状態を反映
 */
void FileDiffWidget::refrectScrollBarH()
{
	refrectScrollBar();
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

/**
 * @brief スクロール位置の行と桁を設定
 * @param cur_row
 * @param cur_col
 * @param scr_row
 * @param scr_col
 */
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
