
#include "TextEditorView.h"
#include "InputMethodPopup.h"
#include <common/qmisc.h>
#include "unicode.h"
#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QFile>
#include <QFontDatabase>
// #include <QKeyEvent>
#include <QMenu>
#include <QPainter>
#include <QScrollBar>
#include <QTimer>
#include <functional>
#include "../Profile.h"

using namespace texteditor;

constexpr int cursor_animation_cycle = 10;

struct TextEditorView::Private {
	
	QScrollBar *scroll_bar_v = nullptr; // TextEditorWidgetが所有する縦スクロールバーへの非所有参照
	QScrollBar *scroll_bar_h = nullptr; // TextEditorWidgetが所有する横スクロールバーへの非所有参照

	TextEditorThemePtr theme; // 本文・行番号・diff・現在行アクセントなど、View固有の配色

	int wheel_delta = 0; // wheelEvent()で40単位の行移動へ変換するまで保持する未消化delta

	bool is_focus_frame_visible = false; // trueならフォーカス時にウィジェット外周の枠を描く

	unsigned int idle_count = 0; // 100ms timerの無変更回数。5回ごとにidle()を通知する

	int cursor_animation_counter = 0; // カーソル表示位相。入力・移動時にcycleへ戻す

	int max_text_width_px = 0; // 最後のpaintEvent()で観測した最長表示行幅（ピクセル）
	
	QTimer update_scroll_bar_timer; // 最大行幅の変化後、横スクロール範囲更新を遅延実行する
	QTimer resize_timer; // 連続resize完了後に折り返し幅を反映するデバウンスタイマー
};

TextEditorView::TextEditorView(QWidget *parent)
	: QWidget(parent)
	, m(new Private)
{
	size_t n = sizeof(LineIndexMap::ValueItem);
	
#ifdef Q_OS_WIN

	{
		QFont font("MS Gothic", 15);
		set_fixed_font(font);
	}
	{
		QFont font("MS PGothic", 15);
		set_text_font(font);
	}

#else

	{
		QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
		font.setPointSize(16);
		setFont(font);
	}
	{
		QFont font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
		font.setPointSize(16);
		set_text_font(font);
	}

#endif

	initEditor();

	setAttribute(Qt::WA_InputMethodEnabled);
#ifdef Q_OS_WIN
	//	m->ime_popup = new InputMethodPopup();
	//	m->ime_popup->setFont(font());
	//	m->ime_popup->setPreEditText(PreEditText());
#endif

	setContextMenuPolicy(Qt::DefaultContextMenu);

	showLineNumber(false);

	update_cursor_rect(true);

	m->cursor_animation_counter = cursor_animation_cycle;
	startTimer(100);
	m->resize_timer.setSingleShot(true);
	connect(&m->resize_timer, &QTimer::timeout, this, [this](){
		layoutEditor();
	});
}

TextEditorView::~TextEditorView()
{
	delete m;
}

/**
 * @brief 基準文字幅を取得する
 * @return
 */
int TextEditorView::basic_character_width_px() const
{
	// 固定幅フォントの '0' を基準文字幅とする
	return fixed_font().basis_char_width();
}
	
void TextEditorView::setTheme(TextEditorThemePtr const &theme)
{
	m->theme = theme;
}

TextEditorTheme const *TextEditorView::theme() const
{
	if (!m->theme) {
		const_cast<TextEditorView *>(this)->setTheme(TextEditorTheme::Light());
	}
	return m->theme.get();
}

void AbstractTextEditorApplication::loadExampleFile()
{
#ifdef Q_OS_WIN
	QString path = "C:/develop/ore/example.txt";
#elif defined(Q_OS_MAC)
	QString path = "/Users/soramimi/develop/ore/example.txt";
#else
	QString path = "/tmp/example.txt";
#endif
	openFile(path);
}

bool TextEditorView::event(QEvent *event)
{
	if (event->type() == QEvent::Polish) {
		updateVisibility();
	}
	return QWidget::event(event);
}

/**
 * @brief 行の高さ
 * @return
 */


static inline QString appendUnicode(QString const &s, char32_t u)
{
	return s + QString::fromUcs4(&u, 1);
}

/**
 * @brief 全文字のX座標を計算する
 * @param chars
 * @param fm
 */
void TextEditorView::_calc_pos_x(CharBuffer *chars, TextEditorContext const *cx, Font const &fixed_tm, Font const &text_tm)
{
	// 通常文字は行頭または直前のタブからの文字列としてまとめて計測し、
	// kerningなどを含む実際の描画幅を各Characterの左右座標へ記録する。
	// タブは等幅フォントを基準に次のタブストップへ進め、そこで計測区間をリセットする。
	int base_x = 0;
	int left_x = 0;
	QString text;
	for (size_t i = 0; i < chars->size(); i++) {
		(*chars)[i].left_x = left_x;
		char32_t u = (*chars)[i].unicode;
		if (u == '\t') {
			int right_x = left_x;
			int tab_indent = fixed_tm.basis_char_width() * cx->tab_indent_size;
			if (tab_indent > 0) {
				right_x = (right_x / tab_indent + 1) * tab_indent;
			}
			(*chars)[i].right_x = right_x;
			base_x = left_x = right_x;
			text.clear();
		} else {
			text = appendUnicode(text, u);
			int right_x = base_x + text_tm.text_width(text);
			(*chars)[i].right_x = right_x;
			left_x = right_x;
		}
	}
}

void TextEditorView::calc_pos_x(CharBuffer *chars) const
{
	_calc_pos_x(chars, cx(), fixed_font(), text_font());
}

/**
 * @brief 物理行番号からLinePropertyを取得する
 * @param vrow 物理行番号
 * @return LinePropertyへのポインタ。vrowが範囲外の場合はnullptr
 */
Document::LineProperty const *TextEditorView::property_of_visual_line(row_index_t vrow) const
{
	if (vrow >= 0 && vrow < visual_nlines()) {
		// parse_line()は有効なrevisionのキャッシュがあれば再利用する。
		// flagsは描画時にcharsと同じ添字で安全に参照できる長さへ揃える。
		CharBuffer *chars = parse_line(vrow);
		Document::Line const *line = visual_line(vrow);
		if (!chars || !line) return nullptr;
		Document::LineProperty *detail = line->detail();
		detail->flags.resize(detail->chars.size());
		return detail;
	}
	return nullptr;
}

PositionX TextEditorView::pos_x_px(row_index_t vrow, col_index_t vcol) const
{
	Document::LineProperty const *line = property_of_visual_line(vrow);
	if (!line) {
		// 空Documentでは実在する表示行はないが、行番号1と同様に編集開始位置
		// (0, 0)を仮想行として描画する。本文原点を返さないとカーソルが
		// x=0付近へ移動し、行番号領域との境界クリップで見えなくなる。
		PositionX ret;
		if (visual_nlines() == 0) {
			assert(vrow == 0 && vcol == 0);
			ret.visual_x_px = linenum_area_width_px() - scroll_horz_pos_px();
			ret.absolute_x_px = 0;
		}
		return ret;
	}
	
	PositionX ret;
	
	ret.absolute_x_px = 0; // 行番号表示領域とスクロール位置を考慮しない絶対X座標
	
	if (vcol > 0 && vcol - 1 < (col_index_t)line->chars.size()) {
		ret.absolute_x_px = (int)line->chars[vcol - 1].right_x;
	}

	ret.visual_x_px = ret.absolute_x_px + linenum_area_width_px() - scroll_horz_pos_px(); // 行番号表示領域とスクロール位置を考慮したX座標
	
	return ret;
}

/**
 * @brief ビューのピクセル座標から行と桁を求める
 * @param pt
 * @return
 */
texteditor::RowCol TextEditorView::vpos_from_px(QPoint const &pt)
{
	TextEditorContext *cx = this->cx();
	const int y = pt.y() / line_height_px();
	const row_index_t vrow = y + scroll_vert_pos_px() - cx->viewport_org_y_rows;
	const int max_vrow = visual_nlines();
	if (vrow >= max_vrow) {
		// 最終行より下だったら、最終行の列数を返す
		RowCol t;
		t.row = max_vrow - 1;
		if (max_vrow > 0) {
			CharBuffer const *chars = parse_line(t.row);
			if (chars && !chars->empty()) {
				t.col = (int)chars->size();
			}
		}
		return t;
	}
	const int x = pt.x() + scroll_horz_pos_px() - linenum_area_width_px();
	CharBuffer const *chars = nullptr;
	Document::LineProperty const *line = property_of_visual_line(vrow);
	if (line) {
		chars = &line->chars;
		if (chars) {
			size_t end = chars->size();
			int left = 0;
			for (size_t col = 0; col < end; col++) {
				int right = (*chars)[col].right_x;
				if (x < right) {
					int l = left - x;
					int r = right - x;
					if (l * l < r * r) {
						return RowCol(vrow, (int)col);
					} else {
						return RowCol(vrow, (int)col + 1);
					}
				}
				left = right;
			}
			while (end > 0 && ((*chars)[end - 1] == '\r' || (*chars)[end - 1] == '\n')) {
				end--;
			}
			return RowCol((int)vrow, (int)end);
		}
	}
	return {};
}

/**
 * @brief 行位置を変更する
 * @param row
 * @param auto_scroll
 * @param by_mouse
 */
void TextEditorView::setCursorRow(row_index_t row, bool auto_scroll, bool by_mouse)
{
	AbstractTextEditorApplication::setCursorRow(row, false, by_mouse);

	// ピクセル座標を更新
	cx()->current_visual_y_px = (cx()->viewport_org_y_rows + cursor_row_px()) * line_height_px();

	// ピクセル座標から桁位置を再計算する
	int x = cx()->current_pos.visual_x_px;
	int y = cx()->current_visual_y_px;
	auto cr = vpos_from_px({x, y});

	set_current_visual_col(cr.col); // 桁位置

	updateSelectionAnchor2(auto_scroll);
}

PositionX TextEditorView::currentPixelX() const
{
	// 水平ピクセル座標
	return pos_x_px(current_visual_row(), current_visual_col());
}

void TextEditorView::bind_scroll_bar(QScrollBar *vsb, QScrollBar *hsb)
{
	m->scroll_bar_v = vsb;
	m->scroll_bar_h = hsb;

#if 0
	connect(m->scroll_bar_v, &QScrollBar::sliderPressed, this, [this]() {
		m->dragging_scroll_bar = m->scroll_bar_v;
	});
	connect(m->scroll_bar_v, &QScrollBar::sliderReleased, this, [this]() {
		m->dragging_scroll_bar = nullptr;
		updateScrollBarRange();
	});

	connect(m->scroll_bar_h, &QScrollBar::sliderPressed, this, [this]() {
		m->dragging_scroll_bar = m->scroll_bar_h;
	});
	connect(m->scroll_bar_h, &QScrollBar::sliderReleased, this, [this]() {
		m->dragging_scroll_bar = nullptr;
	});
#endif
}

void TextEditorView::setup_for_log_widget(TextEditorThemePtr const &theme)
{
	setTheme(theme);
	set_auto_layout(true);
	setTerminalMode(true);
	layoutEditor();
}

void TextEditorView::update_cursor_rect(bool auto_scroll)
{
	if (auto_scroll) {
		update_horz_scroll();
	}

	int x = linenum_area_width_px() + cursor_col_px();
	int y = cx()->viewport_org_y_rows + cursor_row_px();
	y *= line_height_px();
	QPoint pt = QPoint(x, y);
	int w = 1;
	int h = line_height_px();
	cx()->cursor_rect = QRect(pt.x(), pt.y(), w, h);

	QApplication::inputMethod()->update(Qt::ImCursorRectangle);
}

void TextEditorView::updateScrollBarRange()
{
	bool fixedwidth = (wrappingMode() != TextEditorView::WrappingMode::NoWrap);
	
	QScrollBar *vsb = m->scroll_bar_v;
	QScrollBar *hsb = m->scroll_bar_h;
	
	if (vsb) {
		vsb->blockSignals(true);
		vsb->setRange(0, std::max<row_index_t>(0, visual_nlines() - cx()->viewport_height_rows / 2));
		vsb->setPageStep(editor_viewport_height());
		vsb->setValue(scroll_vert_pos_px());
		vsb->blockSignals(false);
	}

	if (hsb) {
		hsb->blockSignals(true);
		if (fixedwidth) {
			hsb->setRange(0, 0);
			hsb->setPageStep(0);
			hsb->setValue(0);
		} else {
			auto half_of_textarea_width_px = (client_width_px() - linenum_area_width_px()) / 2; // テキスト表示領域の半分の幅
			int w = m->max_text_width_px - half_of_textarea_width_px; // テキスト表示領域の半分の幅を引くことで、スクロールバーの最大値を調整する
			hsb->setRange(0, w);
			hsb->setPageStep(w / 10);
			hsb->setValue(scroll_horz_pos_px());
		}
		hsb->setVisible(!fixedwidth);
		hsb->blockSignals(false);
	}

	emit updateScrollBar();
}

void TextEditorView::updateVisibility(update_visibility_option_t const &arg)
{
	if (arg.ensure_current_line_visible) {
		ensure_current_line_visible();
	}

	update_cursor_rect(arg.auto_scroll);

	if (isPaintingSuppressed()) {
		return;
	}
	
	if (cx()->cache.scroll_bar_update_needed) {
		updateScrollBarRange();
		cx()->cache.scroll_bar_update_needed = false;
	}

	m->cursor_animation_counter = cursor_animation_cycle;
	update();
	
	if (arg.emit_event) {
		emit moved(current_visual_row(), current_visual_col(), scroll_vert_pos_px(), scroll_horz_pos_px());
	}
}

std::pair<row_index_t, row_index_t> TextEditorView::visibleRowAndCount()
{
	row_index_t row_start = scrollTopRow();
	row_index_t remaining = std::max<row_index_t>(0, visual_nlines() - row_start);
	row_index_t row_count = std::min(editor_cx->viewport_height_rows, remaining);

	return std::make_pair(row_start, row_count);
}

void TextEditorView::move(int cur_row, int cur_col, int scr_y_px, int scr_x_px, bool auto_scroll)
{
	if (isWidthFixed()) {
		scr_x_px = 0;
	}
	if ((cur_row >= 0 && current_visual_row() != cur_row) || (cur_col >= 0 && current_visual_col() != cur_col) || scroll_vert_pos_px() != scr_y_px || scroll_horz_pos_px() != scr_x_px) {
		if (cur_row >= 0) set_current_visual_row(cur_row);
		if (cur_col >= 0) set_current_visual_col(cur_col);
		if (scr_y_px >= 0) set_scroll_vert_pos_px(scr_y_px);
		if (scr_x_px >= 0) set_scroll_horz_pos_px(scr_x_px);
		update_visibility_option_t opt;
		opt.ensure_current_line_visible = false;
		opt.auto_scroll = auto_scroll;
		updateVisibility(opt);
	}
}

QFont TextEditorView::fixedFont() const
{
	return fixed_font().font();
}

QFont TextEditorView::textFont() const
{
	return text_font().font();
}

void TextEditorView::drawText(QPainter *painter, int px, int py, QString const &str)
{
	painter->drawText(px, py + line_baseline_px(), str);
}

QColor TextEditorView::defaultForegroundColor()
{
	return theme()->fg_default;
}

QColor TextEditorView::defaultBackgroundColor()
{
	return theme()->bg_default;
}

QColor TextEditorView::colorForIndex(CharAttr const &attr, bool foreground)
{
	if (foreground && attr.color.isValid()) {
		return attr.color;
	}
	switch (attr.index) {
	case CharAttr::Invert:
		return foreground ? defaultBackgroundColor() : defaultForegroundColor();
	}
	return foreground ? defaultForegroundColor() : Qt::transparent;//defaultBackgroundColor();
}

void TextEditorView::drawFocusFrame(QPainter *pr)
{
	misc::drawFrame(pr, 0, 0, width(), height(), QColor(0, 128, 255, 128));
	misc::drawFrame(pr, 1, 1, width() - 2, height() - 2, QColor(0, 128, 255, 64));
}

/**
 * @brief TextEditorView::scrollTopRow
 * @return 
 */
int TextEditorView::scrollTopRow() const
{
	return editor_cx->scroll_vert_pos_px;
}

int TextEditorView::view_y_from_vrow(row_index_t vrow) const
{
	return (editor_cx->viewport_org_y_rows + vrow - scrollTopRow()) * line_height_px();
}

/**
 * @brief 行と桁からビュー座標を求める
 * @param row
 * @param col
 * @return
 */
TextEditorView::PointInView TextEditorView::pointInView(int row, int col) const
{
	PointInView pt;
	pt.height = line_height_px();
	pt.y = view_y_from_vrow(row);
	pt.x = pos_x_px(row, col).visual_x_px; // 行と桁位置から水平座標を求める
	return pt;
}

QColor TextEditorView::cursorColor() const
{
	bool blink_on = m->cursor_animation_counter >= cursor_animation_cycle / 2;
	return blink_on ? Qt::white : Qt::transparent;
}

/**
 * @brief カーソルを描画
 * @param row
 * @param col
 * @param pr
 * @param color
 */
void TextEditorView::drawCursor(int row, int col, QPainter *pr)
{
	QColor color = cursorColor();
	PointInView pt = pointInView(row, col);
	pr->fillRect(pt.x -1, pt.y, 2, pt.height, color);
	pr->fillRect(pt.x - 2, pt.y, 4, 2, color);
	pr->fillRect(pt.x - 2, pt.y + pt.height - 2, 4, 2, color);
}

/**
 * @brief 現在位置にカーソルを描画
 * @param pr
 * @param color
 */
void TextEditorView::drawCursor(QPainter *pr)
{
	drawCursor(current_visual_row(), current_visual_col(), pr);
}



void TextEditorView::paintEvent(QPaintEvent *)
{
	bool has_focus = hasFocus();

	// まずウィジェット全体を既定色で消去し、その上へ文書内・文書末尾以降・
	// diff・選択・文字・カーソル・行番号の順で重ねていく。
	QPainter pr(this);
	pr.setFont(textFont());
	pr.fillRect(0, 0, width(), height(), defaultBackgroundColor());
	
	// viewport_org_x_colsから求める本文の左端。左余白を0に設定した場合は0となる。
	const int linenum_width_px = linenum_area_width_px();
	// 論理的な本文X=0をウィジェット座標へ移した位置。NoWrapの水平スクロール分だけ左へ動く。
	const int text_origin_x_px = linenum_width_px - scroll_horz_pos_px();
	
	// Document、LineIndexMap、現在位置など、今回の描画で共通して参照する状態。
	TextEditorContext *cx = editor_cx.get();
	
	// NoWrapではLineIndexMapを構築しないため論理行数をそのまま表示行数とする。
	// Wrap時は各論理行の折り返し数を合計したLineIndexMapの値を使う。
	auto total_visual_row_count = [&]()-> uint64_t {
		if (wrappingMode() == WrappingMode::NoWrap) {
			return logical_nlines();
		} else {
			return cx->line_index_map.total_visual_row_count();
		}
	};
	
	// 行番号と本文の縦境界。本文原点より2px左へ置き、境界線の右側に
	// カーソルや文字を描き始めるための小さな余白を確保する。
	const int vsplit_x = linenum_width_px - 2;
	// vsplit_xからウィジェット右端までの背景描画幅。
	const int text_area_w = width() - vsplit_x;

	// 文書全体に存在する表示行数。空Documentでは0であり、仮想行は含まない。
	const uint64_t total_rows = total_visual_row_count();
	// 名前にpxが残っているが、scroll_vert_pos_px()の実態はビューポート上端の表示行番号。
	// 負値をuint64_tへ変換して巨大値にしないよう、描画計算では0へ丸める。
	const uint64_t scroll_row = scroll_vert_pos_px() > 0
		? static_cast<uint64_t>(scroll_vert_pos_px()) : 0;
	// ビューポート上端から文書末尾までに実在する表示行数。末尾以降では0。
	const uint64_t remaining_rows = total_rows > scroll_row ? total_rows - scroll_row : 0;
	// 空DocumentでもpaintLineNumbers()は編集開始位置として行番号1を描く。
	// 実在する残り行数は0のまま保ち、先頭にいる場合だけ仮想1行分の背景を描画する。
	const uint64_t paint_rows = total_rows == 0 && scroll_row == 0 ? 1 : remaining_rows;
	// 0除算とゼロ高の描画矩形を避けるため、背景計算上の行高は最低1pxとする。
	const int line_height = std::max(1, line_height_px());
	// ウィジェットを覆うのに必要な最大行数より1行多い安全な閾値。
	// paint_rowsがこれ以上なら、乗算せずbottom_yをheight()へ固定して桁あふれを防ぐ。
	const uint64_t visible_capacity = static_cast<uint64_t>(std::max(0, height())) / line_height + 1;
	// 通常の文書背景を塗る下端Y。末尾が画面内なら行高の合計に境界線1pxを加え、
	// 文書が画面下まで続く場合はウィジェット下端へ固定する。
	const int bottom_y = paint_rows >= visible_capacity
		? height()
		: std::min(height(), static_cast<int>(paint_rows * line_height + 1));

	if (bottom_y > 0) {
		// テキスト領域の背景
		pr.fillRect(vsplit_x, 0, text_area_w, bottom_y, theme()->bg_default);
		
		// 行番号領域の背景
		pr.fillRect(0, 0, vsplit_x, bottom_y, theme()->bg_line_number);
	}
	// 終端以降
	if (bottom_y < height()) {
		pr.fillRect(0, bottom_y, width(), height() - bottom_y, theme()->bg_diff_unknown);
	}

	// 選択と本文が行番号領域へはみ出さないように使う、本文専用クリップ矩形。
	auto TextAreaRectForClip = [&](){
		return QRect(linenum_width_px, 0, width() - linenum_width_px, height());
	};
	
	{
		const int line_height = line_height_px();
		
		QTextOption opt;
		opt.setWrapMode(QTextOption::NoWrap);

		// 選択範囲
		SelectionAnchor selection_lower = selection_start();
		SelectionAnchor selection_upper = selection_end();
		if (selection_lower && selection_upper) {
			if (selection_lower > selection_upper) {
				std::swap(selection_lower, selection_upper);
			}
		} else {
			selection_lower = {};
			selection_upper = {};
		}

		int max_text_width_px = 0;
		// 行ごとに完結させるのではなく、全表示行を3回走査して重なり順を固定する。
		// pass 0: 行背景、pass 1: 選択背景、pass 2: 文字と現在行の下線。
		// これにより、後続行の背景が前の行の選択や文字を上書きしない。
		for (int pass = 0; pass < 3; pass++) {
			int view_row = 0; // 描画行番号（ビューポートの左上隅を0とした行位置）
			row_index_t vrow = scrollTopRow(); // 行インデックス（view_row位置に描画すべき論理行インデックス）
			for (int i = 0; i < (int)editor_cx->viewport_height_rows && vrow < visual_nlines(); i++) {
				Document::LineProperty const *formatted_line = property_of_visual_line(vrow);
				if (formatted_line) {
					const QRect rect_line(vsplit_x, view_y_from_vrow(vrow), text_area_w, line_height_px()); // 行全体の矩形
					const QRect rect_text(0, view_y_from_vrow(vrow), width(), line_height_px()); // テキスト領域矩形
					
					const bool iscurrentline = has_focus && vrow == editor_cx->current_visual_row; // 現在の行？
					const int text_origin_y = view_row * line_height; // テキスト原点座標Y（ピクセル単位）
					
					CharBuffer const &chars = formatted_line->chars;
					std::vector<CharFlags> const &flags = formatted_line->flags;
					
					// 背景の描画
					auto DrawBackground = [&](){
						{ // diff差分背景
							Document::LineType type = visual_line(vrow)->sp->meta.type;
							auto FillBG = [&](QColor color){
								pr.fillRect(rect_text, color);
							};
							switch (type) {
							case Document::LineType::Add:     FillBG(theme()->bg_diff_line_add); break; // 追加された行の背景
							case Document::LineType::Del:     FillBG(theme()->bg_diff_line_del); break; // 削除された行の背景
							case Document::LineType::Invalid: FillBG(theme()->bg_diff_unknown);  break;
							}
						}
					};
	
					// 現在行の背景
					auto DrawCurrentLineBackground = [&](){
						pr.fillRect(rect_line, QColor(0, 0, 0));
					};
	
					// 現在行の前景
					auto DrawCurrentLineForeground = [&](){
						int N = 1;
						int x = rect_line.x();
						int y = rect_line.y() + rect_line.height() - N;
						int w = rect_line.width();
						int h = N;
						pr.fillRect(x, y, w, h, theme()->fg_cursor); // アンダーライン
					};
	
					// 選択範囲
					auto DrawSelectionArea = [&](){
						int left_x = 0;
						int right_x = 0;
						if (!chars.empty()) {
							right_x = chars.back().right_x;
						}
						// NoWrapではLineIndexMapが空なので、モードを考慮する共通変換を使う。
						RowCol vlower = visual_position(selection_lower);
						RowCol vupper = visual_position(selection_upper);
						if (vlower.row > vrow) {
							right_x = 0;
						} else if (vupper.row < vrow) {
							right_x = 0;
						} else {
							auto Do = [&](int xpos, RowCol vpos, row_index_t vrow){
								if (vpos.row == vrow) {
									if (vpos.col > 0 && vpos.col - 1 < (col_index_t)chars.size()) {
										xpos = chars[vpos.col - 1].right_x;
									} else {
										xpos = 0;
									}
								}
								return xpos;
							};
							left_x = Do(left_x, vlower, vrow);
							right_x = Do(right_x, vupper, vrow);
						}
						if (left_x < right_x) {
							int x = text_origin_x_px + left_x;
							int y = text_origin_y;
							int w = right_x - left_x;
							int h = line_height;
							pr.save();
							pr.setClipRect(TextAreaRectForClip());
							pr.fillRect(x, y, w, h, QBrush(QColor(64, 128, 128)));
							pr.restore();
						}
					};
	
					// テキスト描画
					auto DrawText = [&](){
						int left_x = 0;
						int right_x = 0;
						std::size_t j = 0;
						pr.save();
						pr.setFont(textFont());
						pr.setClipRect(TextAreaRectForClip());
						while (j < chars.size()) {
							int n = 0;
							QString text;
							while (j + n < chars.size()) {
								if (n == 0) {
									left_x = chars[j].left_x;
									right_x = chars[j].right_x;
								} else { // 2文字目以降
									if (right_x != chars[j + n].left_x) break; // x座標がつながっていないなら抜ける
									if (chars[j].attr != chars[j + n].attr) break; // 属性が異なっていたら抜ける
									if (flags[j].all != flags[j + n].all) break; // フラグが異なっていたら抜ける
								}
								auto u = chars[j + n].unicode;
								if (u == '\t') {
									auto IsDiffMarker = [&](int value) {
										return flags[j].diff_marker == value;
									};
									if (IsDiffMarker(CharFlags::Del) || IsDiffMarker(CharFlags::Add)) {
										// 文字差分フラグがあるとき背景を描く
										int x = text_origin_x_px + left_x;
										int w = right_x - left_x;
										int h = line_height;
										auto DrawDiffMarker = [&](QColor const &color){
											const int N = 6;
											pr.fillRect(x, text_origin_y + h - N, w, N, color);
										};
										if (IsDiffMarker(CharFlags::Del)) {
											DrawDiffMarker(theme()->bg_diff_char_del);
										}
										if (IsDiffMarker(CharFlags::Add)) {
											DrawDiffMarker(theme()->bg_diff_char_add);
										}
									}
									break; // タブなら抜ける
								}
								text = appendUnicode(text, u); // 文字を追加
								right_x = chars[j + n].right_x;
								n++;
							}
							if (!text.isEmpty() && left_x < right_x) {
								auto IsDiffMarker = [&](int value) {
									return flags[j].diff_marker == value;
								};
								pr.setPen(defaultForegroundColor()); // 文字色
								int x = text_origin_x_px + left_x;
								int w = right_x - left_x;
								int h = line_height;
								auto DrawDiffMarker = [&](QColor const &color){
									const int N = 6;
									pr.fillRect(x, text_origin_y + h - N, w, N, color);
								};
								if (IsDiffMarker(CharFlags::Del)) {
									DrawDiffMarker(theme()->bg_diff_char_del); // 削除された文字の下線
								}
								if (IsDiffMarker(CharFlags::Add)) {
									DrawDiffMarker(theme()->bg_diff_char_add); // 追加された文字の下線
								}
								pr.drawText(QRect(x, text_origin_y, w, h), text, opt); // テキスト描画
							}
							if (n == 0) {
								n = 1;
							}
							j += n;
						}
						max_text_width_px = std::max(max_text_width_px, right_x);
						pr.restore();
					};
	
					switch (pass) {
					case 0:
						DrawBackground();
						if (vrow_to_lrow(vrow) == current_logical_row()) {
							DrawCurrentLineBackground();
						}
						break;
					case 1:
						DrawSelectionArea();
						break;
					case 2:
						DrawText();
						if (iscurrentline) {
							DrawCurrentLineForeground();
						}
						break;
					}
				}
				view_row++;
				vrow++;
			}
		}
		if (m->max_text_width_px != max_text_width_px) {
			m->max_text_width_px = max_text_width_px;
			need_to_update_scroll_bar();
			m->update_scroll_bar_timer.stop();
			m->update_scroll_bar_timer.singleShot(100, [this](){
				updateScrollBarRange();
			});
		}
	}
	
	if (bottom_y > 0) {
		// 行番号領域の左側の黒線
		if (linenum_width_px > 0) {
			pr.fillRect(0, 0, 1, bottom_y, Qt::black);
		}
		
		// 行番号領域の右側の黒線
		pr.fillRect(vsplit_x, 0, 1, bottom_y, Qt::black);
	}
	if (bottom_y < height()) {
		// 終端の横線
		pr.fillRect(0, bottom_y, width(), 1, Qt::black);
	}
	
	// フォーカス枠描画
	if (m->is_focus_frame_visible && has_focus) {
		drawFocusFrame(&pr);
	}

	// カーソル描画
	if (has_focus) {
		pr.save();
		auto x = linenum_width_px - 2;
		pr.setClipRect(x, 0, width() - x, bottom_y);
		drawCursor(&pr);
		pr.restore();
	}

	// 行番号描画
	{
		const int bottom = -1;

		paintLineNumbers([&](int y, QString const &text, Document::Line const *vline){
			if (bottom >= 0 && y > bottom) return;

			pr.setBackground(Qt::transparent);
			pr.setPen(theme()->fg_line_number);
			pr.setFont(fixedFont());
			drawText(&pr, 0, y * line_height_px(), text);
			if (vline) {
				char const *mark = nullptr;
				if (vline->sp->meta.type == Document::LineType::Add) {
					mark = "+";
				} else if (vline->sp->meta.type == Document::LineType::Del) {
					mark = "-";
				}
				if (mark) {
					pr.setPen(theme()->fg_default);
					drawText(&pr, linenum_width_px - basic_character_width_px() * 3 / 2, y * line_height_px(), mark);
				}
			}
		});
	}
}

void TextEditorView::moveCursorByMouse()
{
	QPoint mousepos = mapFromGlobal(QCursor::pos()); // マウス座標をグローバルからローカルへ変換
	RowCol pos = vpos_from_px(mousepos); // ローカルマウス座標からカーソル座標へ変換

	// row
	if (pos.row < 0) {
		pos.row = 0;
	} else {
		row_index_t max_vrow = visual_nlines();
		max_vrow = max_vrow > 0 ? (max_vrow - 1) : 0;
		pos.row = std::min(pos.row, max_vrow);
	}
	setCursorPosByMouse(pos, mousepos);
	
	update_visibility_option_t opt;
	opt.ensure_current_line_visible = false;
	opt.auto_scroll = false;
	updateVisibility(opt);
}

void TextEditorView::mousePressEvent(QMouseEvent *event)
{
	if (event->button() == Qt::RightButton) return;

	auto saved_pos = savePos();

	bool shift = (event->modifiers() & Qt::ShiftModifier);
	if (shift) {
		if (hasSelection()) {
			setSelectionAnchor(true, false, false);
		} else {
			setSelectionAnchor(true, true, false);
		}
	}

	moveCursorByMouse();

	if (shift) {
		setSelectionAnchor(true, false, false);
	} else {
		setSelectionAnchor(true, true, false);
	}
	sync_selection();

	if (isTerminalMode()) {
		restorePos();
	}
}

void TextEditorView::mouseReleaseEvent(QMouseEvent * /*event*/)
{
	// マウスボタンを離したときに選択範囲が空だったら選択を解除する
	if (!selection_end() || !selection_start() || selection_start() == selection_end()) {
		clear_selection();
		update();
	}
}

void TextEditorView::mouseMoveEvent(QMouseEvent * /*event*/)
{
	auto saved_pos = savePos();

	moveCursorByMouse();

	setSelectionAnchor(true, true, false);

	if (isTerminalMode()) {
		restorePos();
	}
}

QVariant TextEditorView::inputMethodQuery(Qt::InputMethodQuery q) const
{
	if (q == Qt::ImCursorRectangle) {
		QRect r = cx()->cursor_rect;
		return r;
	}
	return QWidget::inputMethodQuery(q);
}

void TextEditorView::inputMethodEvent(QInputMethodEvent *e)
{
#ifdef Q_OS_WIN
	PreEditText preedit;
	preedit.text = e->preeditString();
	for (QInputMethodEvent::Attribute const &a : e->attributes()) {
		if (a.type == QInputMethodEvent::TextFormat) {
			QTextFormat f = qvariant_cast<QTextFormat>(a.value);
			if (f.type() == QTextFormat::CharFormat) {
				preedit.format.emplace_back(a.start, a.length, f);
			} else {
			}
		}
	}
	if (preedit.text.isEmpty()) {
		//		m->ime_popup->hide();
	} else {
		QPoint pt = mapToGlobal(cx()->cursor_rect.topLeft());
		//		m->ime_popup->move(pt);
		//		m->ime_popup->setFont(font());
		//		m->ime_popup->setPreEditText(preedit);
		//		m->ime_popup->show();
	}
#endif
	QString const &commit_text = e->commitString();
	if (!commit_text.isEmpty()) {
		write_(commit_text, true);
	}
}

void TextEditorView::reflectScrollBar()
{
	int v = m->scroll_bar_v ? m->scroll_bar_v->value() : -1;
	int h = m->scroll_bar_h ? m->scroll_bar_h->value() : -1;
	// qDebug() << v << h;
	move(-1, -1, v, h, false);
}

void TextEditorView::layoutEditor()
{
	if (isAutoLayout()) {
		int h = height();
		int w = width();
		set_client_size(w, h, false);
		
		int content_width_px = w - linenum_area_width_px();
		setContentWidth(content_width_px);
		
		update_visual_lines_all();
		
		updateVisibility();
	}
	AbstractTextEditorApplication::layoutEditor();
}

void TextEditorView::resizeEvent(QResizeEvent * /*event*/)
{
	if (isAutoLayout()) {
		set_client_size(width(), height(), false);
		AbstractTextEditorApplication::layoutEditor();
		m->resize_timer.start(75);
	}
	updateScrollBarRange();
}

void TextEditorView::wheelEvent(QWheelEvent *event)
{
	int pos = 0;
	m->wheel_delta += event->angleDelta().y();
	while (m->wheel_delta >= 40) {
		m->wheel_delta -= 40;
		pos--;
	}
	while (m->wheel_delta <= -40) {
		m->wheel_delta += 40;
		pos++;
	}
	if (m->scroll_bar_v) {
		m->scroll_bar_v->setValue(m->scroll_bar_v->value() + pos);
	}
}

void TextEditorView::setFocusFrameVisible(bool f)
{
	m->is_focus_frame_visible = f;
}

void TextEditorView::timerEvent(QTimerEvent *)
{
	if (!isChanged()) {
		m->idle_count++;
		if (m->idle_count >= 5) {
			m->idle_count = 0;
			emit idle();
		}
	}
	
	constexpr bool cursor_blink_enabled = true;
	if (cursor_blink_enabled) { // カーソル点滅
		bool f = m->cursor_animation_counter >= cursor_animation_cycle / 2;
		if (m->cursor_animation_counter > 0) {
			m->cursor_animation_counter--;
		} else {
			m->cursor_animation_counter = cursor_animation_cycle;
		}
		bool g = m->cursor_animation_counter >= cursor_animation_cycle / 2;
		if (f != g) {
			update();
		}
	}
	
	// qDebug() << m->dragging_scroll_bar;
}

void TextEditorView::contextMenuEvent(QContextMenuEvent *event)
{
	QMenu menu;
	QAction *a_cut = nullptr;
	if (!is_read_only() && !isTerminalMode()) a_cut = menu.addAction("Cut");
	QAction *a_copy = menu.addAction("Copy");
	QAction *a_paste = nullptr;
	if (!is_read_only() && !isTerminalMode()) a_paste = menu.addAction("Paste");
	QAction *a = menu.exec(misc::contextMenuPos(this, event));
	if (a) {
		if (a == a_cut) {
			edit_cut();
			return;
		}
		if (a == a_copy) {
			edit_copy();
			return;
		}
		if (a == a_paste) {
			edit_paste();
			return;
		}
	}
}

void TextEditorView::debug()
{
#if 0
	edit_copy();
	QString str = qApp->clipboard()->text();
	qDebug() << str;
#else
	qApp->clipboard()->setText("Hello,\n world");
	edit_paste();
#endif
}
