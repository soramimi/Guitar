
#include "FileDiffWidget.h"
#include "ApplicationGlobal.h"
#include "BigDiffWindow.h"
#include "GitDiffManager.h"
#include "MainWindow.h"
#include "Theme.h"
#include <common/joinpath.h>
#include <common/misc.h>
#include <common/q/helper.h>
#include "dtl/dtl.hpp"
#include "ui_FileDiffWidget.h"
#include <QBuffer>
#include <QDebug>
#include <QFile>
#include <QKeyEvent>
#include <QMenu>
#include <QPainter>
#include <QStyle>
#include <memory>

enum {
	DiffIndexRole = Qt::UserRole,
};

struct FileDiffWidget::Private {
	FileDiffWidget::InitParam_ init_param_;
	std::vector<std::string> original_lines;
	TextEditorEngine_sp engine_left;
	TextEditorEngine_sp engine_right;
	TextEditorEngine_sp engine_inline;
	TextDiffLineList left_lines;
	TextDiffLineList right_lines;
	TextDiffLineList inline_lines;
	int max_line_length = 0;

	int term_cursor_row = 0;
	int term_cursor_col = 0;

	int scroll_value_v;
	int scroll_value_h;
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
	m->engine_inline = std::make_shared<TextEditorEngine>();
	ui->widget_diff_inline->setDiffMode(m->engine_inline, ui->verticalScrollBar_inline, ui->horizontalScrollBar_inline);

	setViewType(FileViewType::None);
	setViewStyle(ViewStyle::SideBySideTextDiff);
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

FileDiffWidget::ViewStyle FileDiffWidget::viewstyle() const
{
	return m->init_param_.view_style;
}

void FileDiffWidget::setViewStyle(ViewStyle style)
{
	m->init_param_.view_style = style;
	
	if (style == ViewStyle::InlineTextDiff) {
		ui->verticalScrollBar_inline->setValue(m->scroll_value_v);
		ui->horizontalScrollBar_inline->setValue(m->scroll_value_h);
		ui->stackedWidget->setCurrentWidget(ui->page_inline);
	} else {
		ui->verticalScrollBar->setValue(m->scroll_value_v);
		ui->horizontalScrollBar->setValue(m->scroll_value_h);
		ui->stackedWidget->setCurrentWidget(ui->page_side_by_side);
	}
	
	reflectScrollBar();
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

	ui->widget_diff_inline->bind(this, ui->verticalScrollBar_inline, ui->horizontalScrollBar_inline, mainwindow()->themeForTextEditor());
	
	connect(ui->verticalScrollBar_inline, &QAbstractSlider::valueChanged, this, &FileDiffWidget::onVerticalScrollValueChanged_inline);
	connect(ui->horizontalScrollBar_inline, &QAbstractSlider::valueChanged, this, &FileDiffWidget::onHorizontalScrollValueChanged_inline);
}

void FileDiffWidget::setViewType(FileViewType type)
{
	ui->widget_diff_left->setViewType(type);
	ui->widget_diff_right->setViewType(type);
	ui->widget_diff_inline->setViewType(type);
}

void FileDiffWidget::setMaximizeButtonEnabled(bool f)
{
	ui->toolButton_fullscreen->setVisible(f);
	ui->toolButton_fullscreen->setEnabled(f);
}

GitRunner FileDiffWidget::git()
{
	if (!mainwindow()) {
		qDebug() << "Maybe, you forgot to call FileDiffWidget::bind()?";
		return {};
	}
	return mainwindow()->git();
}

GitObject FileDiffWidget::catFile(GitRunner g, std::string const &id)
{
	if (g.isValidWorkingCopy()) {
		static constexpr std::string_view path_prefix = PATH_PREFIX;
		if (misc::starts_with(id, path_prefix)) {
			std::string path = g.workingDir() / id.substr(path_prefix.size());
			QFile file(misc::escape_double_quote_for_file_open((QS)path));
			if (file.open(QFile::ReadOnly)) {
				GitObject obj;
				obj.content = file.readAll();
				return obj;
			}
		} else if (GitHash::isValidID(id)) {
			return g.catFile(GitHash(id));
		}
	}
	return {};
}

int FileDiffWidget::totalTextLines() const
{
	return m->engine_left->document.lines.size();
}

void FileDiffWidget::clearDiffView()
{
	ui->widget_diff_left->clear();
	ui->widget_diff_right->clear();
	ui->widget_diff_inline->clear();
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
	ui->verticalScrollBar_inline->setValue(0);
	ui->horizontalScrollBar_inline->setValue(0);
}

void FileDiffWidget::scrollToBottom()
{
	QScrollBar *sb = ui->verticalScrollBar;
	sb->setValue(sb->maximum());
}

void FileDiffWidget::updateSliderCursor()
{
	if (viewstyle() == SideBySideTextDiff) {
		ui->widget_diff_slider->clear(true);
	}
}

void FileDiffWidget::updateControls()
{
	updateSliderCursor();
}

void FileDiffWidget::makeSideBySideDiffData(GitDiff const &diff, std::vector<std::string> const &original_lines, TextDiffLineList *left_lines, TextDiffLineList *right_lines, TextDiffLineList *inline_lines)
{
	left_lines->clear();
	right_lines->clear();
	inline_lines->clear();

	m->original_lines = original_lines;

	size_t linenum = 0;

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
				while (isdigit((unsigned char)*p)) {
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
		return l.pos < r.pos;
	});
	size_t hunk_number = 0;
	while (linenum < original_lines.size() || hunk_number < hunks.size()) {
		size_t endline = original_lines.size();
		if (hunk_number <  hunks.size()) {
			if (endline > hunks[hunk_number].pos) {
				endline = hunks[hunk_number].pos;
			}
		}
		if (linenum < endline) {
			std::string line = original_lines[linenum];
			std::vector<char> vec(line.begin(), line.end());
			TextDiffLine l(vec, TextDiffLine::Normal);
			left_lines->push_back(l);
			right_lines->push_back(l);
			inline_lines->push_back(l);
			linenum++;
		}
		while (hunk_number < hunks.size()) {
			HunkItem const &hunk = hunks[hunk_number];
			if (hunk.pos > linenum) break;
			std::vector<TextDiffLine> tmp_left;
			std::vector<TextDiffLine> tmp_right;
			std::vector<TextDiffLine> tmp_inline;
			int del = 0;
			int add = 0;
			auto FlushBlank = [&](){
				while (del < add) {
					tmp_left.emplace_back();
					del++;
				}
				while (del > add) {
					tmp_right.emplace_back();
					add++;
				}
				del = add = 0;
			};
			for (std::string line : hunk.lines) {
				if (line.empty()) continue;
				int c = (unsigned char)*line.c_str();
				line = line.substr(1);
				std::vector<char> vec(line.begin(), line.end());
				if (c == '-') {
					del++;
					TextDiffLine l(vec, TextDiffLine::Del);
					l.hunk_number = hunk_number;
					tmp_left.push_back(l);
					tmp_inline.push_back(l);
				} else if (c == '+') {
					add++;
					TextDiffLine l(vec, TextDiffLine::Add);
					l.hunk_number = hunk_number;
					tmp_right.push_back(l);
					tmp_inline.push_back(l);
				} else {
					FlushBlank();
					TextDiffLine l(vec, TextDiffLine::Normal);
					l.hunk_number = hunk_number;
					tmp_left.push_back(l);
					tmp_right.push_back(l);
					tmp_inline.push_back(l);
				}
			}
			FlushBlank();
			auto ComplementNewLine = [](std::vector<TextDiffLine> *lines){
				for (TextDiffLine &line : *lines) {
					size_t n = line.text().size();
					if (n > 0) {
						int c = (unsigned char)line.text().back();
						if (c != '\r' && c != '\n') {
							line.append_text('\n');
						}
					}
				}
			};
			ComplementNewLine(&tmp_left);
			ComplementNewLine(&tmp_right);
			ComplementNewLine(&tmp_inline);
			for (auto it = tmp_left.begin(); it != tmp_left.end(); it++) {
				TextDiffLine l(*it);
				left_lines->push_back(l);
			}
			for (auto it = tmp_right.begin(); it != tmp_right.end(); it++) {
				TextDiffLine l(*it);
				right_lines->push_back(l);
			}
			for (auto it = tmp_inline.begin(); it != tmp_inline.end(); it++) {
				TextDiffLine l(*it);
				inline_lines->push_back(l);
			}
			linenum = hunk.pos + hunk.len;
			hunk_number++;
		}
	}
}

void FileDiffWidget::setDiffText(GitDiff const &diff, TextDiffLineList const &left_lines, TextDiffLineList const &right_lines, TextDiffLineList const &inline_lines)
{
	ASSERT_MAIN_THREAD();
	m->max_line_length = 0;

	enum Pane {
		Left,
		Right,
		InlineTextDiff,
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
				if (pane == Pane::Right || pane == Pane::InlineTextDiff) {
					item.line_number = linenum++;
				}
				break;
			case TextDiffLine::Del:
				if (pane == Pane::Left) {
					item.line_number = linenum++;
				} else if (pane == Pane::InlineTextDiff) {
					item.line_number = 0; // 削除行はインラインビューでは行番号を表示しない
				}
				break;
			default:
				item.line_number = linenum; // 行番号は設定するが、インクリメントはしない
				break;
			}

			out->push_back(item);
		}
	};
	SetLineNumber(left_lines, Pane::Left, &m->left_lines);
	SetLineNumber(right_lines, Pane::Right, &m->right_lines);
	SetLineNumber(inline_lines, Pane::InlineTextDiff, &m->inline_lines);

	ui->widget_diff_left->setText(&m->left_lines, (QS)diff.blob.a_id_or_path, (QS)diff.path);
	ui->widget_diff_right->setText(&m->right_lines, (QS)diff.blob.b_id_or_path, (QS)diff.path);
	ui->widget_diff_inline->setText(&m->inline_lines, {}, (QS)diff.path);
	reflectScrollBar();
	ui->widget_diff_slider->clear(true);
}

bool FileDiffWidget::setSubmodule(GitDiff const &diff)
{
	GitSubmoduleItem const &submod_a = diff.a_submodule.item;
	GitSubmoduleItem const &submod_b = diff.b_submodule.item;
	GitCommitItem const &submod_commit_a = diff.a_submodule.commit;
	GitCommitItem const &submod_commit_b = diff.b_submodule.commit;
	if (submod_a || submod_b) {
		auto Text = [](GitSubmoduleItem const *submodule, GitCommitItem const *submodule_commit, TextDiffLineList *out){
			*out = {};
			if (submodule && *submodule) {
				QString text;
				text += "name: " + QString::fromStdString(submodule->name) + '\n';
				text += "path: " + QString::fromStdString(submodule->path) + '\n';
				text += "url: " + QString::fromStdString(submodule->url) + '\n';
				text += "commit: " + QString::fromStdString(submodule->id.toString()) + '\n';
				text += "date: " + (QS)submodule_commit->commit_date.toString() + '\n';
				text += "author: " + (QS)submodule_commit->author + '\n';
				text += "email: " + (QS)submodule_commit->email + '\n';
				text += '\n';
				text += (QS)submodule_commit->message;
				for (QString const &line : misc::splitLines(text)) {
					out->push_back(Document::Line::View(line.toStdString()));
				}
			}
		};
		TextDiffLineList left_lines;
		TextDiffLineList right_lines;
		TextDiffLineList inline_lines;
		if (submod_a) {
			Text(&submod_a, &submod_commit_a, &left_lines);
		}
		if (submod_b) {
			Text(&submod_b, &submod_commit_b, &right_lines);
		}
		setDiffText(diff, left_lines, right_lines, inline_lines);
		return true;
	}
	return false;
}

/**
 * @brief テキストか画像かでビューを切り替える
 * @return
 */
FileViewType FileDiffWidget::setupPreviewWidget()
{
	clearDiffView();

	std::string mimetype_l = global->mimetype_by_data(m->init_param_.bytes_a);
	std::string mimetype_r = global->mimetype_by_data(m->init_param_.bytes_b);

	if (misc::isImage(mimetype_l) || misc::isImage(mimetype_r)) { // image

		ui->verticalScrollBar->setVisible(false);
		ui->horizontalScrollBar->setVisible(false);
		ui->widget_diff_slider->setVisible(false);

		ui->widget_diff_left->setImage(mimetype_l, m->init_param_.bytes_a, (QS)m->init_param_.diff.blob.a_id_or_path, (QS)m->init_param_.diff.path);
		ui->widget_diff_right->setImage(mimetype_r, m->init_param_.bytes_b, (QS)m->init_param_.diff.blob.b_id_or_path, (QS)m->init_param_.diff.path);

		return FileViewType::Image;

	} else { // text

		ui->verticalScrollBar->setVisible(true);
		ui->horizontalScrollBar->setVisible(true);
		ui->widget_diff_slider->setVisible(true);

		setViewType(FileViewType::Text);
		return FileViewType::Text;
	}
}

// void FileDiffWidget::setSingleFile(QByteArray const &ba, QString const &id, QString const &path)
// {
// 	m->init_param_ = InitParam_();
// 	m->init_param_.view_style = FileDiffWidget::ViewStyle::SingleFile;
// 	m->init_param_.bytes_a = ba;
// 	m->init_param_.diff.path = path.toStdString();
// 	m->init_param_.diff.blob.a_id_or_path = id.toStdString();
// }

void FileDiffWidget::setOriginalLines_(QByteArray const &ba, GitSubmoduleItem const *submodule, GitCommitItem const *submodule_commit)
{
	(void)submodule;
	(void)submodule_commit;

	m->original_lines.clear();

	if (!ba.isEmpty()) {
		std::vector<std::string_view> lines = misc::splitLinesKeepNewLineV(std::string_view{ba.data(), (size_t)ba.size()});
		m->original_lines.clear();
		for (std::string_view const &line : lines) {
			m->original_lines.push_back(std::string(line));
		}
	}
}

void FileDiffWidget::setLeftOnly(GitDiff const &diff, QByteArray const &ba)
{
	m->init_param_ = InitParam_();
	setViewStyle(ViewStyle::LeftOnly);
	
	// m->init_param_.view_style = FileDiffWidget::ViewStyle::LeftOnly;
	m->init_param_.bytes_a = ba;
	m->init_param_.diff = diff;

	setOriginalLines_(ba, &diff.a_submodule.item, &diff.a_submodule.commit);

	if (setupPreviewWidget() == FileViewType::Text) {

		TextDiffLineList left_lines;
		TextDiffLineList right_lines;
		TextDiffLineList inline_lines;

		for (std::string const &line : m->original_lines) {
			left_lines.push_back(TextDiffLine::View(line, TextDiffLine::Del));
			right_lines.push_back(TextDiffLine());
			inline_lines.push_back(TextDiffLine::View(line, TextDiffLine::Del));
		}

		setDiffText(diff, left_lines, right_lines, inline_lines);
	}
}

void FileDiffWidget::setRightOnly(GitDiff const &diff, QByteArray const &ba)
{
	m->init_param_ = InitParam_();
	setViewStyle(ViewStyle::RightOnly);
	
	// m->init_param_.view_style = FileDiffWidget::ViewStyle::RightOnly;
	m->init_param_.bytes_b = ba;
	m->init_param_.diff = diff;

	setOriginalLines_(ba, &diff.b_submodule.item, &diff.b_submodule.commit);

	if (setSubmodule(diff)) {
		// ok
	} else if (setupPreviewWidget() == FileViewType::Text) {

		TextDiffLineList left_lines;
		TextDiffLineList right_lines;
		TextDiffLineList inline_lines;

		for (std::string const &line : m->original_lines) {
			left_lines.push_back(TextDiffLine());
			right_lines.push_back(TextDiffLine::View(line, TextDiffLine::Add));
			inline_lines.push_back(TextDiffLine::View(line, TextDiffLine::Add));
		}

		setDiffText(diff, left_lines, right_lines, inline_lines);
	}
}

void FileDiffWidget::_setTwoFilesDiff(GitDiff const &diff, QByteArray const &ba, bool uncommitted, QString const &workingdir)
{
	m->init_param_ = InitParam_();
	setViewStyle(ViewStyle::SideBySideTextDiff);

	m->init_param_.bytes_a = ba;
	m->init_param_.diff = diff;
	m->init_param_.uncommitted = uncommitted;
	m->init_param_.workingdir = workingdir;

	setOriginalLines_(ba, {}, {});

	if (setSubmodule(diff)) {
		// ok
	} else {
		if (setupPreviewWidget() == FileViewType::Text) {

			TextDiffLineList left_lines;
			TextDiffLineList right_lines;
			TextDiffLineList inline_lines;

			makeSideBySideDiffData(diff, m->original_lines, &left_lines, &right_lines, &inline_lines);

			setDiffText(diff, left_lines, right_lines, inline_lines);
		}
	}
}

void FileDiffWidget::setSideBySideDiff(GitDiff const &diff, QByteArray const &ba, bool uncommitted, QString const &workingdir)
{
	_setTwoFilesDiff(diff, ba, uncommitted, workingdir);
	setViewStyle(ViewStyle::SideBySideTextDiff);
}

void FileDiffWidget::setInlineDiff(GitDiff const &diff, QByteArray const &ba, bool uncommitted, QString const &workingdir)
{
	_setTwoFilesDiff(diff, ba, uncommitted, workingdir);
	setViewStyle(ViewStyle::InlineTextDiff);
}

void FileDiffWidget::setSideBySideBlobDiff(GitDiff const &diff, QByteArray const &ba_a, QByteArray const &ba_b, QString const &workingdir)
{
	m->init_param_ = InitParam_();
	m->init_param_.view_style = FileDiffWidget::ViewStyle::SideBySideImageDiff;
	m->init_param_.bytes_a = ba_a;
	m->init_param_.bytes_b = ba_b;
	m->init_param_.diff = diff;
	m->init_param_.workingdir = workingdir;

	setOriginalLines_(ba_a, {}, {});

	if (setupPreviewWidget() == FileViewType::Text) {

		TextDiffLineList left_lines;
		TextDiffLineList right_lines;
		TextDiffLineList inline_lines;

		makeSideBySideDiffData(m->init_param_.diff, m->original_lines, &left_lines, &right_lines, &inline_lines);

		setDiffText(m->init_param_.diff, left_lines, right_lines, inline_lines);
	}
}

std::string FileDiffWidget::diffObjects(std::string const &a_id, std::string const &b_id)
{
	return GitDiffManager::diffObjects(git(), a_id, b_id);
}

/**
 * @brief コミットIDの検証
 * @param id
 * @return
 */
bool FileDiffWidget::isValidID(std::string const &id)
{
	if (id.size() == 0) {
		return false;
	}
	if (id[0] == PATH_PREFIX[0]) {
		return true;
	}
	return GitHash::isValidID(id);
}

/**
 * @brief 差分ビューを更新
 * @param info
 * @param uncommited
 */
void FileDiffWidget::updateDiffView(GitDiff const &info, bool uncommited)
{
	ASSERT_MAIN_THREAD();
	
	GitRunner g = git();
	if (!g.isValidWorkingCopy()) return;

	if (isValidID(info.blob.a_id_or_path) && isValidID(info.blob.b_id_or_path)) {
		GitObject obj_a = catFile(g, info.blob.a_id_or_path);
		GitObject obj_b = catFile(g, info.blob.b_id_or_path);
		std::string mime_a = global->mimetype_by_data(obj_a.content);
		std::string mime_b = global->mimetype_by_data(obj_b.content);
		if (misc::isImage(mime_a) && misc::isImage(mime_b)) {
			setSideBySideBlobDiff(info, obj_a.content, obj_b.content, QString::fromStdString(g.workingDir()));
			return;
		}
	}

	{
		GitDiff diff;
		if (isValidID(info.blob.a_id_or_path) && isValidID(info.blob.b_id_or_path)) {
			std::string text = diffObjects(info.blob.a_id_or_path, info.blob.b_id_or_path);
			diff = GitDiffManager::parseDiff(text, info);
		} else {
			diff = info;
		}

		GitObject obj;
		if (isValidID(diff.blob.a_id_or_path)) { // 左が有効
			obj = catFile(g, diff.blob.a_id_or_path);
			if (isValidID(diff.blob.b_id_or_path)) { // 右が有効
				// if (diffstyle == DiffViewStyle::SideBySide) {
				// 	// 左右分割diff表示
				// 	setSideBySideDiff(diff, obj.content, uncommited, QString::fromStdString(g.workingDir()));
				// } else if (diffstyle == DiffViewStyle::Inline) {
				// 	// インラインdiff表示
				// 	setInlineDiff(diff, obj.content, uncommited, QString::fromStdString(g.workingDir()));
				// }
				_setTwoFilesDiff(diff, obj.content, uncommited, QString::fromStdString(g.workingDir()));
			} else {
				setLeftOnly(diff, obj.content); // 右が無効の時は、削除されたファイル
			}
		} else if (isValidID(diff.blob.b_id_or_path)) { // 左が無効で右が有効の時は、追加されたファイル
			obj = catFile(g, diff.blob.b_id_or_path);
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
void FileDiffWidget::updateDiffView(std::string const &id_left, std::string const &id_right, std::string const &path)
{
	GitRunner g = git();
	if (!g.isValidWorkingCopy()) return;

	std::string text = diffObjects(id_left, id_right);
	
	GitDiff diff;
	{
		GitDiff info;
		info.path = path;
		info.blob.a_id_or_path = id_left;
		info.blob.b_id_or_path = id_right;
		info.mode = "100644";
		diff = GitDiffManager::parseDiff(text, info);
	}

	GitObject obj = catFile(g, diff.blob.a_id_or_path);
	setSideBySideDiff(diff, obj.content, false, QString::fromStdString(g.workingDir()));

	ui->widget_diff_slider->clear(false);

	resetScrollBarValue();
	updateControls();

	ui->widget_diff_slider->update();
}

void FileDiffWidget::resizeEvent(QResizeEvent *)
{
	reflectScrollBar();
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
	ui->verticalScrollBar_inline->setValue(value);
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

// void FileDiffWidget::onScrollValueChanged2(int value)
// {
// 	ui->verticalScrollBar->setValue(value);
// }

void FileDiffWidget::onDiffWidgetResized()
{
	updateControls();
}

void FileDiffWidget::on_toolButton_fullscreen_clicked()
{
	if (m->init_param_.diff.blob.a_id_or_path.empty() && m->init_param_.diff.blob.b_id_or_path.empty()) {
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
	int total = 0;
	int value = 0;
	int page = 0;
	if (viewstyle() == SideBySideTextDiff) {
		total = m->engine_left->document.lines.size();
		value = ui->verticalScrollBar->value();
		page = ui->verticalScrollBar->pageStep();
	} else if (viewstyle() == InlineTextDiff) {
		total = m->engine_inline->document.lines.size();
		value = ui->verticalScrollBar_inline->value();
		page = ui->verticalScrollBar_inline->pageStep();
		
	}
	ui->widget_diff_slider->setScrollPos(total, value, page);
}

/**
 * @brief スクロールバーの状態を反映
 * @param updateformat
 */
void FileDiffWidget::reflectScrollBar()
{
	if (viewstyle() == SideBySideTextDiff) {
		ui->widget_diff_left->reflectScrollBar();
		ui->widget_diff_right->reflectScrollBar();
	} else if (viewstyle() == InlineTextDiff) {
		ui->widget_diff_inline->reflectScrollBar();
	}

	const bool updateformat = true;
	if (updateformat) {

		// 左と右のテキストを取得
		TextEditorView::FormattedLines *llines = ui->widget_diff_left->getTextEditorView()->fetchLines2(false);
		TextEditorView::FormattedLines *rlines = ui->widget_diff_right->getTextEditorView()->fetchLines2(false);
		ui->widget_diff_inline->getTextEditorView()->fetchLines2(false);

		// サイドバイサイドのとき文字差分を求める
		if (viewstyle() == SideBySideTextDiff) {
			using Char = AbstractCharacterBasedApplication::Char;
			using Attr = AbstractCharacterBasedApplication::CharAttr;

			const size_t n = std::min(llines->size(), rlines->size());

			for (std::size_t i = 0; i < n; i++) {
				auto Find = [&](TextEditorView::FormattedLines *lines, FileViewWidget *w)-> std::vector<Char> * {
					int row = i + w->texteditor()->scrollTopRow();
					auto it = lines->lines.find(row);
					return it == lines->lines.end() ? nullptr : it->second.chars.get();
				};
				std::vector<Char> *lchars = Find(llines, ui->widget_diff_left);
				std::vector<Char> *rchars = Find(rlines, ui->widget_diff_right);
				if (!lchars) continue;
				if (!rchars) continue;
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
void FileDiffWidget::reflectScrollBarV()
{
	reflectScrollBar();
}

/**
 * @brief 横スクロールバーの状態を反映
 */
void FileDiffWidget::reflectScrollBarH()
{
	reflectScrollBar();
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
	reflectScrollBar();
	onUpdateSliderBar();
}

void FileDiffWidget::on_toolButton_menu_clicked()
{
	QMenu menu;
	QAction *a_sydebyside = menu.addAction("Side by side");
	QAction *a_inline = menu.addAction("Inline");
	QAction *a = menu.exec(QCursor::pos() + QPoint(8, -8));
	if (a) {
		if (a == a_sydebyside) {
			setViewStyle(ViewStyle::SideBySideTextDiff);
			return;
		}
		if (a == a_inline) {
			setViewStyle(ViewStyle::InlineTextDiff);
			return;
		}
	}
}

void FileDiffWidget::onVerticalScrollValueChanged(int value)
{
	m->scroll_value_v = value;
	reflectScrollBarV();
}

void FileDiffWidget::onHorizontalScrollValueChanged(int value)
{
	m->scroll_value_h = value;
	reflectScrollBarH();
}

void FileDiffWidget::onVerticalScrollValueChanged_inline(int value)
{
	m->scroll_value_v = value;		
	reflectScrollBarV();
}

void FileDiffWidget::onHorizontalScrollValueChanged_inline(int value)
{
	m->scroll_value_h = value;
	reflectScrollBarH();
}



