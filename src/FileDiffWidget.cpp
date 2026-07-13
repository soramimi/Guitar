
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
#include <Profile.h>

enum {
	DiffIndexRole = Qt::UserRole,
};

struct HunkItem {
	int hunk_number = -1;
	size_t pos = 0;
	size_t len = 0;
	std::vector<std::string> lines;
};

struct FileDiffWidget::Private {
	FileDiffWidget::InitParam_ init_param_;
	
	TextEditorEngine_sp engine_left;
	TextEditorEngine_sp engine_right;
	TextEditorEngine_sp engine_inline;
	
	std::vector<HunkItem> hunks;
	
	FileDiffWidget::TextDiffData diffdata;
	
	int max_line_length = 0;

	int term_cursor_row = 0;
	int term_cursor_col = 0;

	int scroll_value_v = 0;
	int scroll_value_h = 0;
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

bool FileDiffWidget::isSideBySideView() const
{
	// return viewstyle() == SideBySideTextDiff || viewstyle() == LeftOnly || viewstyle() == RightOnly;
	return viewstyle() != InlineTextDiff;
}

void FileDiffWidget::setViewStyle(ViewStyle style)
{
	m->init_param_.view_style = style;
	
	if (isSideBySideView()) {
		ui->verticalScrollBar->setValue(m->scroll_value_v);
		ui->horizontalScrollBar->setValue(m->scroll_value_h);
		ui->stackedWidget->setCurrentWidget(ui->page_side_by_side);
	} else {
		ui->verticalScrollBar_inline->setValue(m->scroll_value_v);
		ui->horizontalScrollBar_inline->setValue(m->scroll_value_h);
		ui->stackedWidget->setCurrentWidget(ui->page_inline);
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
	return (int)m->engine_left->document.lines.size();
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
	m->scroll_value_v = 0;
	m->scroll_value_h = 0;
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
	if (isSideBySideView()) {
		ui->widget_diff_slider->clear(true);
	}
}

void FileDiffWidget::updateControls()
{
	updateSliderCursor();
}

FileDiffWidget::TextDiffData FileDiffWidget::makeSideBySideDiffData(GitDiff const &diff, std::vector<std::string> const &original_lines)
{
	TextDiffData ret;
	m->diffdata.original_lines = original_lines;
	
	size_t linenum = 0;
	
	m->hunks.clear();
	{
		m->hunks.reserve(diff.hunks.size());
		int number = 0;
		for (auto it = diff.hunks.begin(); it != diff.hunks.end(); it++, number++) {
			std::string_view at = it->at;
			if (misc::starts_with(at, "@@ -")) {
				size_t pos = 0;
				size_t len = 0;
				char const *p = at.data() + 4;
				auto ParseNumber = [&](){
					size_t v = 0;
					char const *end = at.data() + at.size();
					while (p < end && isdigit((unsigned char)*p)) {
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
				item.lines = it->lines;
				m->hunks.push_back(item);
			}
		}
	}
	
	size_t hunk_number = 0;
	while (linenum < m->diffdata.original_lines.size() || hunk_number < m->hunks.size()) {
		size_t endline = m->diffdata.original_lines.size();
		if (hunk_number <  m->hunks.size()) {
			auto next_hunk_start = m->hunks[hunk_number].pos;
			endline = std::min(endline, next_hunk_start);
		}
		if (linenum < endline) {
			std::string_view line = m->diffdata.original_lines[linenum];
			TextDiffLine l = TextDiffLine::View(line, Document::LineType::Normal);
			ret.left_lines.push_back(l);
			ret.right_lines.push_back(l);
			ret.inline_lines.push_back(l);
			linenum++;
		}
		while (hunk_number < m->hunks.size()) {
			HunkItem const &hunk = m->hunks[hunk_number];
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
			for (std::string_view line : hunk.lines) {
				if (line.empty()) continue;
				char c = line[0];
				line = line.substr(1);
				if (c == '-') {
					del++;
					TextDiffLine item = TextDiffLine::View(line, Document::LineType::Del);
					tmp_left.push_back(item);
					tmp_inline.push_back(item);
				} else if (c == '+') {
					add++;
					TextDiffLine item = TextDiffLine::View(line, Document::LineType::Add);
					tmp_right.push_back(item);
					tmp_inline.push_back(item);
				} else {
					FlushBlank();
					TextDiffLine item = TextDiffLine::View(line, Document::LineType::Normal);
					tmp_left.push_back(item);
					tmp_right.push_back(item);
					tmp_inline.push_back(item);
				}
			}
			FlushBlank();
			
			for (auto const &line : tmp_left)   ret.left_lines.push_back(line);
			for (auto const &line : tmp_right)  ret.right_lines.push_back(line);
			for (auto const &line : tmp_inline) ret.inline_lines.push_back(line);
			
			linenum = hunk.pos + hunk.len;
			hunk_number++;
		}
	}
	
	return ret;
}

void FileDiffWidget::setDiffText(GitDiff const &diff, TextDiffData const &data)
{
	ASSERT_MAIN_THREAD();
	m->max_line_length = 0;

	enum Pane {
		Left,
		Right,
		Inline,
	};
	auto SetLineNumber = [&](TextDiffLineList const &lines, Pane pane){
		TextDiffLineList ret;
		int linenum = 1;
		for (TextDiffLine const &line : lines) {
			TextDiffLine item = line;

			switch (item.type) {
			case Document::LineType::Normal:
				item.line_number = linenum++;
				break;
			case Document::LineType::Add:
				if (pane == Pane::Right || pane == Pane::Inline) {
					item.line_number = linenum++;
				}
				break;
			case Document::LineType::Del:
				if (pane == Pane::Left) {
					item.line_number = linenum++;
				} else if (pane == Pane::Inline) {
					item.line_number = 0; // 削除行はインラインビューでは行番号を表示しない
				}
				break;
			default:
				item.line_number = linenum; // 行番号は設定するが、インクリメントはしない
				break;
			}

			ret.push_back(item);
		}
		return ret;
	};
	m->diffdata.left_lines = SetLineNumber(data.left_lines, Pane::Left);
	m->diffdata.right_lines = SetLineNumber(data.right_lines, Pane::Right);
	m->diffdata.inline_lines = SetLineNumber(data.inline_lines, Pane::Inline);

	ui->widget_diff_left->setText(&m->diffdata.left_lines, (QS)diff.blob.a_id_or_path, (QS)diff.path);
	ui->widget_diff_right->setText(&m->diffdata.right_lines, (QS)diff.blob.b_id_or_path, (QS)diff.path);
	ui->widget_diff_inline->setText(&m->diffdata.inline_lines, {}, (QS)diff.path);
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
		auto Text = [](GitSubmoduleItem const *submodule, GitCommitItem const *submodule_commit){
			TextDiffLineList ret;
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
					ret.push_back(Document::Line::View(line.toStdString()));
				}
			}
			return ret;
		};
		TextDiffData data;
		if (submod_a) data.left_lines = Text(&submod_a, &submod_commit_a);
		if (submod_b) data.right_lines = Text(&submod_b, &submod_commit_b);
		setDiffText(diff, data);
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

void FileDiffWidget::setOriginalLines_(QByteArray const &ba, GitSubmoduleItem const *submodule, GitCommitItem const *submodule_commit)
{
	(void)submodule;
	(void)submodule_commit;

	m->diffdata.original_lines.clear();

	if (!ba.isEmpty()) {
		std::vector<std::string_view> lines = misc::splitLinesKeepNewLineV(std::string_view{ba.data(), (size_t)ba.size()});
		m->diffdata.original_lines.clear();
		for (std::string_view const &line : lines) {
			m->diffdata.original_lines.push_back(std::string(line));
		}
	}
}

void FileDiffWidget::_setDiff(ViewStyle viewstyle, GitDiff const &diff, QByteArray const &ba, bool uncommitted, QString const &workingdir)
{
	m->init_param_ = {};
	setViewStyle(viewstyle);
	
	if (viewstyle == ViewStyle::LeftOnly) {
		m->init_param_.bytes_a = ba;
		setOriginalLines_(ba, &diff.a_submodule.item, &diff.a_submodule.commit);
	} else if (viewstyle == ViewStyle::RightOnly) {
		m->init_param_.bytes_b = ba; // right side
		setOriginalLines_(ba, &diff.b_submodule.item, &diff.b_submodule.commit);
	} else {
		m->init_param_.bytes_a = ba;
	}
	
	m->init_param_.diff = diff;
	m->init_param_.uncommitted = uncommitted;
	m->init_param_.workingdir = workingdir;
	
	setOriginalLines_(ba, {}, {});
	
	if (setSubmodule(diff)) {
		// done
	} else if (setupPreviewWidget() == FileViewType::Text) {
		TextDiffData data;
		if (viewstyle == ViewStyle::SideBySideTextDiff || viewstyle == ViewStyle::InlineTextDiff) {
			data = makeSideBySideDiffData(diff, m->diffdata.original_lines);
		} else if (viewstyle == ViewStyle::LeftOnly) {
			for (std::string const &line : m->diffdata.original_lines) {
				data.left_lines.push_back(TextDiffLine::View(line, Document::LineType::Del));
				data.right_lines.push_back(TextDiffLine()); // right side is empty
				data.inline_lines.push_back(TextDiffLine::View(line, Document::LineType::Del));
			}
		} else if (viewstyle == ViewStyle::RightOnly) {
			for (std::string const &line : m->diffdata.original_lines) {
				data.left_lines.push_back(TextDiffLine()); // left side is empty
				data.right_lines.push_back(TextDiffLine::View(line, Document::LineType::Add));
				data.inline_lines.push_back(TextDiffLine::View(line, Document::LineType::Add));
			}
		}
		setDiffText(diff, data);
	}
	
	setViewStyle(viewstyle);
	resetScrollBarValue();
}

void FileDiffWidget::setLeftOnlyDiff(GitDiff const &diff, QByteArray const &ba, bool uncommited, QString const &workingdir)
{
	_setDiff(ViewStyle::LeftOnly, diff, ba, uncommited, workingdir);
}

void FileDiffWidget::setRightOnlyDiff(GitDiff const &diff, QByteArray const &ba, bool uncommited, QString const &workingdir)
{
	_setDiff(ViewStyle::RightOnly, diff, ba, uncommited, workingdir);
}

void FileDiffWidget::setSideBySideDiff(GitDiff const &diff, QByteArray const &ba, bool uncommitted, QString const &workingdir)
{
	_setDiff(ViewStyle::SideBySideTextDiff, diff, ba, uncommitted, workingdir);
}

void FileDiffWidget::setInlineDiff(GitDiff const &diff, QByteArray const &ba, bool uncommitted, QString const &workingdir)
{
	_setDiff(ViewStyle::InlineTextDiff, diff, ba, uncommitted, workingdir);
}

void FileDiffWidget::setSideBySideBlobDiff(GitDiff const &diff, QByteArray const &ba_a, QByteArray const &ba_b, bool uncommitted, QString const &workingdir)
{
	m->init_param_ = {};
	m->init_param_.view_style = FileDiffWidget::ViewStyle::SideBySideBinaryDiff;
	m->init_param_.bytes_a = ba_a;
	m->init_param_.bytes_b = ba_b;
	m->init_param_.diff = diff;
	m->init_param_.uncommitted = uncommitted;
	m->init_param_.workingdir = workingdir;

	setOriginalLines_(ba_a, {}, {});

	if (setupPreviewWidget() == FileViewType::Text) {
		TextDiffData data = makeSideBySideDiffData(m->init_param_.diff, m->diffdata.original_lines);
		setDiffText(m->init_param_.diff, data);
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
	
	const auto workingdir = QString::fromStdString(g.workingDir());
	
	if (isValidID(info.blob.a_id_or_path) && isValidID(info.blob.b_id_or_path)) {
		GitObject obj_a = catFile(g, info.blob.a_id_or_path);
		GitObject obj_b = catFile(g, info.blob.b_id_or_path);
		std::string mime_a = global->mimetype_by_data(obj_a.content);
		std::string mime_b = global->mimetype_by_data(obj_b.content);
		if (misc::isImage(mime_a) && misc::isImage(mime_b)) {
			setSideBySideBlobDiff(info, obj_a.content, obj_b.content, uncommited, workingdir);
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
				_setDiff(SideBySideTextDiff, diff, obj.content, uncommited, workingdir);
			} else {
				setLeftOnlyDiff(diff, obj.content, uncommited, workingdir); // 右が無効の時は、削除されたファイル
			}
		} else if (isValidID(diff.blob.b_id_or_path)) { // 左が無効で右が有効の時は、追加されたファイル
			obj = catFile(g, diff.blob.b_id_or_path);
			setRightOnlyDiff(diff, obj.content, uncommited, workingdir);
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
	if (isSideBySideView()) {
		total = (int)m->engine_left->document.lines.size();
		value = ui->verticalScrollBar->value();
		page = ui->verticalScrollBar->pageStep();
	} else if (viewstyle() == InlineTextDiff) {
		total = (int)m->engine_inline->document.lines.size();
		value = ui->verticalScrollBar_inline->value();
		page = ui->verticalScrollBar_inline->pageStep();
		
	}
	ui->widget_diff_slider->setScrollPos(total, value, page);
}

/**
 * @brief 文字単位の差分を求める
 * @param row 行番号
 * @param count 行数
 * @param llines 左の行データ
 * @param rlines 右の行データ
 */
static void characterWiseDiff(int row, int count, TextEditorView::FormattedLines *left_lines, TextEditorView::FormattedLines *right_lines)
{
	using Char = AbstractCharacterBasedApplication::Char;
	using Attr = CharAttr;
	
	for (int i = 0; i < count; i++) {
		struct Line {
			std::vector<Char> *chrs = nullptr;
			std::vector<Attr> *atts = nullptr;
		};
		auto FindLine = [&](TextEditorView::FormattedLines *lines, int row)-> Line {
			Line ret;
			auto it = lines->lines.find(row);
			if (it != lines->lines.end()) {
				ret.chrs = it->second.chars.get();
				ret.atts = it->second.atts2.get();
			}
			return ret;
		};
		Line left = FindLine(left_lines, row + i);
		Line right = FindLine(right_lines, row + i);
		if (!left.chrs) continue;
		if (!right.chrs) continue;
		
		// 文字差分を求める
		// dtl (diff template library) https://github.com/cubicdaiya/dtl
		size_t l = 0;
		size_t r = 0;
		if (1) {
			dtl::Diff<char32_t, std::vector<Char>> d(*left.chrs, *right.chrs);
			d.compose();
			
			auto const &sq = d.getSes().getSequence();
			for (std::pair<char32_t, dtl::elemInfo> const &t : sq) {
				switch (t.second.type) {
				case dtl::SES_COMMON:
					l++;
					r++;
					break;
				case dtl::SES_DELETE:
					left.chrs->at(l).attr.flags |= Attr::Underline1;
					l++;
					break;
				case dtl::SES_ADD:
					right.chrs->at(r).attr.flags |= Attr::Underline2;
					r++;
					break;
				}
			}
		} else { // ↓なんかいまいち...
			
			struct Token {
				size_t pos = 0;
				std::vector<Char> vec;
				int compare(Token const &r) const
				{
					size_t i = 0;
					while (1) {
						char32_t c = (i < vec.size()) ? vec[i].unicode : 0;
						char32_t d = (i < r.vec.size()) ? r.vec[i].unicode : 0;
						if (c < d) return -1;
						if (c > d) return 1;
						if (c == 0) return 0;
						i++;
					}
				}
				bool operator < (const Token &r) const
				{
					return compare(r) < 0;
				}
				bool operator == (const Token &r) const
				{
					return compare(r) == 0;
				}
			};
			auto Tokenize = [&](std::vector<Char> const &in)-> std::vector<Token> {
				std::vector<Token> ret;
				size_t i = 0;
				while (i < in.size()) {
					Token token;
					token.pos = i;
					size_t j = i;
					char32_t c = in[i].unicode;
					if (c < 0x80 && isdigit(c)) {
						do {
							j++;
							c = (j < in.size()) ? in[j].unicode : 0;
						} while (isdigit(c));
					} else if (c < 0x80 && islower(c)) {
						do {
							j++;
							c = (j < in.size()) ? in[j].unicode : 0;
						} while (islower(c));
					} else if (c < 0x80 && isupper(c)) {
						do {
							j++;
							c = (j < in.size()) ? in[j].unicode : 0;
						} while (isalpha(c));
					} else {
						j++;
					}
					while (i < j) {
						token.vec.push_back(in[i++].unicode);
					}
					ret.push_back(token);
				}
				return ret;
			};
			
			std::vector<Token> token_left = Tokenize(*left.chrs);
			std::vector<Token> token_right = Tokenize(*right.chrs);
			
			dtl::Diff<Token, std::vector<Token>> d(token_left, token_right);
			d.compose();
			
			auto const &sq = d.getSes().getSequence();
			for (std::pair<Token, dtl::elemInfo> const &t : sq) {
				switch (t.second.type) {
				case dtl::SES_COMMON:
					l++;
					r++;
					break;
				case dtl::SES_DELETE:
					{
						size_t pos = token_left[l].pos;
						for (size_t i = 0; i < token_left[l].vec.size(); i++) {
							left.chrs->at(pos + i).attr.flags |= Attr::Underline1;
						}
					}
					l++;
					break;
				case dtl::SES_ADD:
					{
						size_t pos = token_right[r].pos;
						for (size_t j = 0; j < token_right[r].vec.size(); j++) {
							right.chrs->at(pos + j).attr.flags |= Attr::Underline2;
						}
					}
					r++;
					break;
				}
			}
		}
	}
}

/**
 * @brief スクロールバーの状態を反映
 * @param updateformat
 */
void FileDiffWidget::reflectScrollBar()
{
	if (isSideBySideView()) {
		ui->widget_diff_left->reflectScrollBar();
		ui->widget_diff_right->reflectScrollBar();
	} else if (viewstyle() == InlineTextDiff) {
		ui->widget_diff_inline->reflectScrollBar();
	}

	if (isSideBySideView()) {
		// サイドバイサイドのとき文字差分を求める
		auto *left = ui->widget_diff_left->getTextEditorView()->fetchLines2(false);
		auto *right = ui->widget_diff_right->getTextEditorView()->fetchLines2(false);
		Q_ASSERT(left->row_start == ui->widget_diff_left->getTextEditorView()->scrollTopRow());
		Q_ASSERT(right->row_start == ui->widget_diff_right->getTextEditorView()->scrollTopRow());
		Q_ASSERT(left->row_start == right->row_start); // 左右のスクロール位置は同じであるはず
		int count = std::max(left->row_count, right->row_count);
		characterWiseDiff(left->row_start, count, left, right); // 文字差分を求める
	} else if (viewstyle() == InlineTextDiff) {
		ui->widget_diff_inline->getTextEditorView()->fetchLines2(false);
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
	if (pane == DiffPane::Left)  return Do(m->diffdata.left_lines);
	if (pane == DiffPane::Right) return Do(m->diffdata.right_lines);
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



