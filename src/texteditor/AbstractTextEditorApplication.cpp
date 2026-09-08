#include "AbstractTextEditorApplication.h"
#include "UnicodeWidth.h"
#include "unicode.h"
#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QFile>
#include <atomic>
#include <common/misc.h>
#include <memory>
#include <thread>

using WriteMode = AbstractTextEditorApplication::WriteMode;
using FormattedLine = AbstractTextEditorApplication::FormattedLine;

// このファイルで扱う行データの関係:
//
//   Document::logical_lines                 文書の正本
//     -> Line::Meta::visual_lines            論理行ごとの折り返し結果
//     -> TextEditorContext::line_index_map   論理座標 <-> 表示座標の索引
//     -> Line::Meta::detail                  UTF-8解析・文字位置計算キャッシュ
//
// 表示行の平坦なコピーは持たない。表示行vrowはLineIndexMapで
// (logical row, wrap index)へ変換し、論理行内のvisual_linesを直接参照する。

class EsccapeSequence {
private:
	int offset = 0;            // 現在受信中のエスケープシーケンス長
	unsigned char data[100];   // 受信中シーケンスの固定長バッファ
	int color_fg = -1;         // ANSI前景色番号（-1は既定色）
	int color_bg = -1;         // ANSI背景色番号（-1は既定色）
public:
	bool isActive() const
	{
		return offset > 0;
	}
	void write(char c)
	{
		if (c == 0x1b) {
			data[0] = c;
			offset = 1;
			return;
		}
		data[offset] = c;
		if (offset > 0) {
			if (c == 'm') {
				if (data[1] == '[' && isdigit(data[2]) && isdigit(data[3])) {
					data[offset] = 0;
					if (data[2] == '3') {
						color_fg = atoi((char const *)data + 3);
						if (color_fg == 9) {
							color_fg = -1;
						}
					}
					if (data[2] == '4') {
						color_bg = atoi((char const *)data + 3);
						if (color_bg == 9) {
							color_bg = -1;
						}
					}
				}
				offset = 0;
				return;
			}
			if (offset + 1 < (int)sizeof(data)) {
				offset++;
			}
		}
	}
	int fg_color_code() const
	{
		return color_fg == 9 ? -1 : color_fg;
	}
	int bg_color_code() const
	{
		return color_bg == 9 ? -1 : color_bg;
	}
};

struct AbstractTextEditorApplication::Private {
	bool is_changed = false;        // 保存後に文書が変更されたか
	bool is_read_only = false;      // trueなら文書を変更する操作を拒否する
	bool is_terminal_mode = false;  // 端末出力向けの追記・エスケープ処理を使うか
	bool is_cursor_visible = true;  // カーソル描画を許可するか
	State state = State::Normal;    // エディタの実行状態
	int client_width_px = 1920;     // ウィジェット全体の幅
	int client_height_px = 1080;    // ウィジェット全体の高さ
	int content_width_px = -1;      // 行番号領域を除く折り返し可能幅
	bool auto_layout = false;       // resize時にビューポートと折り返しを更新するか
	QString recently_used_path;     // Open/Saveダイアログの基準パス
	bool show_line_number = true;   // 行番号領域を描画するか
	int left_margin = AbstractTextEditorApplication::LINE_NUMBER_AREA_WIDTH; // 左余白（基準文字数）

	bool is_painting_suppressed = false; // 一括更新中などに再描画を抑える
	int line_margin = 3;                  // カーソル自動スクロール時に確保する上下行数
	WriteMode write_mode = WriteMode::Insert; // 挿入または上書きモード
	Qt::KeyboardModifiers keyboard_modifiers = Qt::KeyboardModifier::NoModifier; // 最後の入力修飾キー
	bool ctrl_modifier = false;           // keyboard_modifiersから展開したCtrl状態
	bool shift_modifier = false;          // keyboard_modifiersから展開したShift状態
	EsccapeSequence escape_sequence;      // terminal mode用ANSIシーケンス解析状態

	bool cursor_moved_by_mouse = false; // terminal modeで末尾追従を判断するための移動元

	// 現在の折り返し規則。変更時は全論理行のvisual_linesとLineIndexMapを再構築する。
	AbstractTextEditorApplication::WrappingMode wrapping_mode = AbstractTextEditorApplication::WrappingMode::NoWrap;

	struct Selection {
		SelectionAnchor start; // 選択開始位置（論理座標）
		SelectionAnchor end;   // 選択終了位置（論理座標）
	};
	Selection selection;
	
	struct {
		AbstractTextEditorApplication::Font fixed; // タブ・基準セル・行番号に使う等幅フォント
		AbstractTextEditorApplication::Font text;  // 本文描画と実際の文字幅計測に使うフォント
	} font;
	
	int top_margin_px = 0;    // 1表示行の上側余白
	int bottom_margin_px = 1; // 1表示行の下側余白
	// fixed/textフォントのいずれかが変わるたび増加する。LinePropertyの
	// metrics_revisionと比較して、文字位置計算結果を再利用できるか判定する。
	uint64_t metrics_revision = 1;
	// 幅・フォント・折り返しモード・文書全体が変化し、全行の再折り返しが必要な状態。
	bool full_wrap_update_needed = true;
};

AbstractTextEditorApplication::AbstractTextEditorApplication()
	: m(new Private)
{
}

AbstractTextEditorApplication::~AbstractTextEditorApplication()
{
	delete m;
}

void AbstractTextEditorApplication::setModifierKeys(Qt::KeyboardModifiers const &keymod)
{
	m->keyboard_modifiers = keymod;
	m->ctrl_modifier = m->keyboard_modifiers & Qt::ControlModifier;
	m->shift_modifier = m->keyboard_modifiers & Qt::ShiftModifier;
}

void AbstractTextEditorApplication::clearShiftModifier()
{
	m->shift_modifier = false;
}

bool AbstractTextEditorApplication::isControlModifierPressed() const
{
	return m->ctrl_modifier;
}

bool AbstractTextEditorApplication::isShiftModifierPressed() const
{
	return m->shift_modifier;
}

void AbstractTextEditorApplication::set_auto_layout(bool f)
{
	m->auto_layout = f;
	layoutEditor();
}

void AbstractTextEditorApplication::showLineNumber(bool show, int left_margin)
{
	m->show_line_number = show;
	m->left_margin = left_margin;
}

void AbstractTextEditorApplication::setCursorVisible(bool show)
{
	m->is_cursor_visible = show;
}

bool AbstractTextEditorApplication::isCursorVisible()
{
	return m->is_cursor_visible;
}

bool AbstractTextEditorApplication::isChanged() const
{
	return m->is_changed;
}

void AbstractTextEditorApplication::setChanged(bool f)
{
	m->is_changed = f;
}

int AbstractTextEditorApplication::leftMargin_() const
{
	return m->left_margin;
}

void AbstractTextEditorApplication::setRecentlyUsedPath(QString const &path)
{
	m->recently_used_path = path;
}

QString AbstractTextEditorApplication::recentlyUsedPath()
{
	return m->recently_used_path;
}

/**
 * @brief 論理行数を返す
 * @return
 */
int AbstractTextEditorApplication::logical_nlines() const
{
	return document()->logical_lines.size();
}

/**
 * @brief 物理行数を返す
 * @return
 */
row_index_t AbstractTextEditorApplication::visual_nlines() const
{
	if (m->wrapping_mode == WrappingMode::NoWrap) {
		// 折り返さない場合は論理行と表示行が1対1なので索引を参照しない。
		return logical_nlines();
	} else {
		TextEditorContext const *cx = this->cx();
		if (cx->cache.nlines == std::nullopt) { // キャッシュが無効化されている場合は再計算する
			// LineIndexMapが保持する各論理行の折り返し数の総和。
			cx->cache.nlines = cx->line_index_map.total_visual_row_count();
		}
		return *cx->cache.nlines;
	}
}

/**
 * @brief スクロールバー更新が必要であることを通知する
 */
void AbstractTextEditorApplication::need_to_update_scroll_bar()
{
	cx()->cache.scroll_bar_update_needed = true;
}

/**
 * @brief 行番号表示領域の幅を返す（ピクセル単位）
 * @return
 */
int AbstractTextEditorApplication::linenum_area_width_px() const
{
	return editor_cx->viewport_org_x_cols * fixedFontMetrics().basis_char_width();
}

/**
 * @brief 物理行数キャッシュを無効化する
 */
void AbstractTextEditorApplication::invalidate_nlines_cache()
{
	cx()->cache.nlines = std::nullopt;
	need_to_update_scroll_bar();
}

void AbstractTextEditorApplication::_update_logical_pos_cache() const
{
	TextEditorContext::Cache *cache = &cx()->cache; // mutable

	row_index_t vrow = current_visual_row();
	col_index_t vcol = current_visual_col();
	
	// カーソルは描画・上下移動に都合のよい表示座標で保持する。
	// 編集操作の直前に、折り返し断片の先頭論理列と表示列を合成して論理座標へ戻す。
	// NoWrapではLineIndexMapを構築しないため、必ずモードを考慮する共通変換を使う。
	auto logical = query_logical_for_visual_row(vrow);
	cache->current_logical_row = logical.lrow;
	cache->current_logical_col = logical.lcol + vcol;
}

row_index_t AbstractTextEditorApplication::current_logical_row() const
{
	_update_logical_pos_cache();
	return cx()->cache.current_logical_row;
}

col_index_t AbstractTextEditorApplication::current_logical_col() const
{
	_update_logical_pos_cache();
	return cx()->cache.current_logical_col;
}

void AbstractTextEditorApplication::set_current_visual_row(row_index_t row)
{
	cx()->current_visual_row = row;
}

void AbstractTextEditorApplication::set_current_visual_col(col_index_t col)
{
	cx()->current_visual_col = col;
}

row_index_t AbstractTextEditorApplication::current_visual_row() const
{
	return cx()->current_visual_row;
}

int AbstractTextEditorApplication::current_visual_col() const
{
	return cx()->current_visual_col;
}

int AbstractTextEditorApplication::current_visual_x_px() const
{
	return cx()->current_visual_x_px;
}

int AbstractTextEditorApplication::scroll_vert_pos_px() const
{
	return cx()->scroll_vert_pos_px;
}

int AbstractTextEditorApplication::scroll_horz_pos_px() const
{
	return cx()->scroll_horz_pos_px;
}

void AbstractTextEditorApplication::set_scroll_vert_pos_px(int row)
{
	cx()->scroll_vert_pos_px = row;
}

void AbstractTextEditorApplication::set_scroll_horz_pos_px(int col)
{
	cx()->scroll_horz_pos_px = col;
}

int AbstractTextEditorApplication::cursor_col_px() const
{
	return current_visual_col() * fixedFontMetrics().basis_char_width() - scroll_horz_pos_px();
}

int AbstractTextEditorApplication::cursor_row_px() const
{
	return current_visual_row() - scroll_vert_pos_px();
}

/**
 * @brief 物理行を取得する
 * @param vrow 物理行番号
 * @return 物理行（存在しない場合はnullptrを返す）
 */
Document::Line *AbstractTextEditorApplication::visual_line(row_index_t vrow)
{
	if (m->wrapping_mode == WrappingMode::NoWrap) {
		std::vector<Document::Line> &lines = document()->logical_lines;
		return (vrow >= 0 && vrow < (row_index_t)lines.size()) ? &lines[vrow] : nullptr;
	}

	if (vrow < 0 || vrow >= visual_nlines()) return nullptr;
	// LineIndexMapは表示行を、論理行とその中の折り返し断片番号へ変換する。
	// 平坦な表示行配列を持たないため、行挿入・削除で後続キャッシュをシフトする必要がない。
	LineIndexMap::LogicalPosition pos = cx()->line_index_map.visual_to_logical(vrow);
	if (pos.lrow >= document()->logical_lines.size()) return nullptr;
	Document::Line &logical = document()->logical_lines[pos.lrow];
	if (pos.wrap_index >= logical.sp->meta.visual_lines.size()) {
		// 通常はLineIndexMapとvisual_linesが同時に更新される。未計算状態から
		// 参照された場合だけ、この論理行を遅延計算して整合させる。
		update_visual_line(pos.lrow, false);
	}
	return pos.wrap_index < logical.sp->meta.visual_lines.size()
		? &logical.sp->meta.visual_lines[pos.wrap_index]
		: nullptr;
}

Document::Line const *AbstractTextEditorApplication::currentLine() const
{
	row_index_t vrow = current_visual_row();
	return (vrow >= 0 && vrow < visual_nlines()) ? visual_line(vrow) : nullptr;
}

//

const SelectionAnchor &AbstractTextEditorApplication::selection_start() const
{
	return m->selection.start;
}

const SelectionAnchor &AbstractTextEditorApplication::selection_end() const
{
	return m->selection.end;
}

void AbstractTextEditorApplication::set_selection_start(const SelectionAnchor &anchor)
{
	m->selection.start = anchor;
}

void AbstractTextEditorApplication::set_selection_end(const SelectionAnchor &anchor)
{
	m->selection.end = anchor;
}

void AbstractTextEditorApplication::set_selection_start_enabled(bool enabled)
{
	m->selection.start.enabled = enabled;
}

void AbstractTextEditorApplication::set_selection_end_enabled(bool enabled)
{
	m->selection.end.enabled = enabled;
}

void AbstractTextEditorApplication::sync_selection()
{
	m->selection.start = m->selection.end;
}

void AbstractTextEditorApplication::clear_selection()
{
	m->selection.start = {};
	m->selection.end = {};
}


/**
 * @brief UTF-8行をCharacter列へ変換し、全文字のX座標を計算する
 *
 * LinePropertyのtext_revisionとmetrics_revisionが一致する場合は計算済みの
 * CharBufferを返す。幅変更だけではこのキャッシュを失効させない。
 * @param cx
 * @param line
 * @return
 */
CharBuffer AbstractTextEditorApplication::_parseLine(TextEditorContext const *cx, Document::Line const *line, std::mutex *mutex) const
{
	if (!line) return {};

	// 解析結果は表示行番号ではなくLine自身に帰属させる。行の挿入・削除で
	// 表示行番号が変化しても、同じLineのキャッシュはそのまま利用できる。
	Document::LineProperty *detail = line->detail();
	if (detail &&
		detail->text_revision == line->sp->meta.text_revision &&
		detail->metrics_revision == m->metrics_revision) {
		return detail->chars;
	}
	
	CharBuffer ret;
	
	std::string_view text = line->text();
	
	int col = 0;
	int len = text.size();
	if (len > 0) {
		ret.reserve(len);
		char const *src = text.data();
		utf8 u8(src, len);
		while (1) {
			int n = 0;
			char32_t c = u8.next();
			if (c == 0) {
				n = 1;
			} else {
				if (c == '\t') {
					int z = nextTabStop(cx, col);
					n = z - col;
				} else {
					n = charWidth(c);
				}
			}
			if (c == 0) break;
			col += n;
			ret.emplace_back(c);
		}
	}
	
	// calc_pos_x()はFont::text_width_cache_を更新する。全行の並列折り返し時は
	// その共有キャッシュへのアクセスだけを直列化する。
	if (mutex) mutex->lock();
	calc_pos_x(&ret); // 内部でキャッシュを更新するのでmutexで保護する必要がある
	if (mutex) mutex->unlock();

	if (!detail) {
		detail = const_cast<Document::Line *>(line)->newDetail();
	}
	detail->chars = ret;
	detail->text_revision = line->sp->meta.text_revision;
	detail->metrics_revision = m->metrics_revision;

	return ret;
}

/**
 * @brief 行を解析してCharacterの配列を返す（全文字のX座標を計算する）
 * @param line
 * @param mutex
 * @return
 */
CharBuffer AbstractTextEditorApplication::_parseLine(Document::Line const *line, std::mutex *mutex) const
{
	return _parseLine(cx(), line, mutex);
}

/**
 * @brief 行を解析してCharacterの配列を返す（全文字のX座標を計算する）
 * @param vrow
 * @return
 */
CharBuffer *AbstractTextEditorApplication::parseLine(row_index_t vrow) const
{
	Document::Line *line = const_cast<AbstractTextEditorApplication *>(this)->visual_line(vrow);
	if (!line) return nullptr;
	_parseLine(line);
	return &line->detail()->chars;
}

/**
 * @brief 物理行番号から論理行番号と論理列番号を求める。
 * @param vrow 物理行番号
 * @return 論理行番号と論理列番号を含むVisualRowInfo構造体
 */
LineIndexMap::LogicalPosition AbstractTextEditorApplication::query_logical_for_visual_row(row_index_t vrow) const
{
	if (vrow < 0) return {};
	
	LineIndexMap::LogicalPosition ret;
	
	if (wrappingMode() == WrappingMode::NoWrap) {
		ret.lrow = std::min(vrow, visual_nlines()); // NoWrapの場合、論理行と物理行は同じ
		ret.lcol = 0;
	} else {
		ret = cx()->line_index_map.visual_to_logical(vrow);
		
	}
	return ret;
}

/**
 * @brief 指定した表示行範囲の描画・解析詳細を無効化する
 * @param vrow 物理行番号
 */
void AbstractTextEditorApplication::invalidate_visual_row_info(row_index_t vrow, size_t n)
{
	invalidate_visual_line_details(vrow, n);
}

void AbstractTextEditorApplication::invalidate_logical_row_info(row_index_t lrow)
{
	// テキスト変更前の折り返し断片に付随する描画詳細だけを破棄する。
	// 論理行本体の解析キャッシュはset_text()がrevision更新とともに破棄する。
	if (lrow >= 0 && lrow < logical_nlines()) {
		Document::Line &line = document()->logical_lines[lrow];
		for (Document::Line &visual : line.sp->meta.visual_lines) {
			visual.clearDetail();
		}
	}
}

void AbstractTextEditorApplication::invalidate_visual_line_details(row_index_t vrow, size_t n)
{
	// 表示行は安定IDではないため、LineIndexMapで現在の断片へ解決してから破棄する。
	// n == size_t(-1) は末尾までを表す。
	if (vrow < 0) return;
	const row_index_t count = visual_nlines();
	const row_index_t end = (n == size_t(-1))
		? count
		: std::min<row_index_t>(count, vrow + static_cast<row_index_t>(n));
	while (vrow < end) {
		Document::Line *line = visual_line(vrow++);
		if (line) line->clearDetail();
	}
}



CharBuffer const *AbstractTextEditorApplication::parseCurrentLine() const
{
	return parseLine(current_visual_row());
}

void AbstractTextEditorApplication::setWrappingMode(WrappingMode mode)
{
	if (m->wrapping_mode == mode) return;
	// 同じ文字列と幅でも分割規則が変わるため、LineIndexMapを含め全再構築する。
	m->wrapping_mode = mode;
	m->full_wrap_update_needed = true;
	update_visual_lines_all();
}

AbstractTextEditorApplication::WrappingMode AbstractTextEditorApplication::wrappingMode() const
{
	return m->wrapping_mode;
}

std::vector<Document::Line> AbstractTextEditorApplication::wrap_line(Document::Line line, std::mutex *mutex) const
{
	if (wrappingMode() == WrappingMode::NoWrap) return {};
	
	const int width_px = m->content_width_px;
	
	std::vector<CharBuffer> chrs_out;
	// _parseLine()はtext/metrics revisionが一致すればデコード・文字幅計算済みの
	// CharBufferを返す。したがって幅だけの変更では分割位置の探索だけをやり直す。
	const CharBuffer chrs_in = _parseLine(&line, mutex);
	
	if (chrs_in.empty()) {
		chrs_out.push_back({});
	} else {
		int left_px = 0;
		int right_px = 0;

		auto Out = [&](size_t i, size_t n){
			CharBuffer chrs;
			for (size_t j = 0; j < n; j++) {
				Character c = chrs_in[i + j];
				c.left_x = right_px - left_px;
				chrs.push_back(c);
			}
			chrs_out.push_back(chrs);
		};
		
		auto IsBreakable = [](char32_t b, char32_t c){ // 分割可能テスト
			if (b < 0x80 && c < 0x80) {
				if (isspace(b) && isspace(c)) return false; // 連続する空白では折り返し不可
				if (isupper(b) && isalnum(c)) return false; // 大文字と英数字の間は折り返し不可
				if (isalnum(b)) {
					if (isupper(c)) return true;  // 小文字と大文字の間は折り返し可
					if (isalnum(c)) return false; // 英数字の間は折り返し不可
					if (isspace(c)) return false; // 英数字に後続する空白では折り返さない
				}
			}
			return true;
		};

		size_t last = 0;
		size_t curr = 0;
		const size_t N = chrs_in.size();
		char32_t b = '\n'; // before
		char32_t c = '\n'; // current
		
		WrappingMode wrapping_mode;
		auto Reset = [&](){
			wrapping_mode = WrappingMode::CharWrap; // 初期状態は文字単位での折り返し
			c = '\n';
		};
		Reset();
		
		while (curr < N) {
			Character const *ch = &chrs_in[curr];
			right_px = ch->right_x;
			b = c;
			c = ch->unicode;
			if (c == '\r') {
				c = '\n';
			}
			size_t next = curr + 1;
			if (wrapping_mode == WrappingMode::CharWrap) {
				// 分割可能な位置に来たら、折り返しモードを単語単位に切り替えます。
				// （最初の分割可能位置までは、常に文字単位での折り返し）
				if (last < curr && IsBreakable(b, c)) {
					wrapping_mode = wrappingMode();
					// WordWrapの場合、そのまま下のifに入る
				}
			}
			if (wrapping_mode == WrappingMode::WordWrap) {
				// 次の分割可能位置を探す
				char32_t d = c;
				while (next <= N) { // next == N means end of line
					b = d;
					d = -1;
					if (next < N) { // next < N means not end of line
						d = chrs_in[next].unicode; // next character
						if (d == '\r') {
							d = '\n';
						}
					}
					if (IsBreakable(b, d)) break;
					right_px = chrs_in[next].right_x;
					next++;
				}
			}

			// 幅が制限を超えた場合、lastからcurr（またはnext）までの行を出力し、lastとcurrを適切に更新します。
			if (right_px - left_px > width_px) {
				if (last < curr) {
					Out(last, curr - last);
					left_px = ch->left_x;
					last = curr;
				} else { // 現在の文字自体が幅の制限を超えている場合、それを出力し、次の文字に移動します。
					Out(last, next - last);
					if (c == '\n') break;
					last = curr = next;
				}
				// 折り返しモードを文字単位に戻します。
				Reset();
			} else { // それ以外の場合、currをnextに移動します。
				if (next == N || c == '\n') { // 次の文字が行末の場合、残りの文字を出力します。
					Out(last, next - last);
					break;
				}
				curr = next;
			}
		}
	}
	
	std::vector<Document::Line> ret;
	{
		// 現段階では各断片を独立したUTF-8 Lineとして保持する。
		// logical_col_pos/lenが論理行との対応を表し、LineIndexMapにもlenを登録する。
		int logical_col = 0;
		for (CharBuffer const &w : chrs_out) {
			std::vector<char> v;
			for (Character const &c : *w.vec) {
				unicode_helper_::encode_utf8(c.unicode, [&](char d){v.push_back(d);});
			}
			Document::Line line(v);
			line.sp->meta.logical_col_pos = logical_col;
			line.sp->meta.logical_col_len = w.size();
			ret.emplace_back(line);
			logical_col += w.size();
		}
	}
	return ret;
}

/**
 * @brief 論理行番号から物理行番号を求める。
 * @param lrow 論理行番号
 * @return 物理行番号
 */
row_index_t AbstractTextEditorApplication::lrow_to_vrow(row_index_t lrow) const
{
	if (wrappingMode() == WrappingMode::NoWrap) {
		return std::min(lrow, visual_nlines());
	}
	
	auto pos = cx()->line_index_map.logical_to_visual(lrow, 0);
	return pos.vrow;
}

row_index_t AbstractTextEditorApplication::vrow_to_lrow(row_index_t vrow) const
{
	if (wrappingMode() == WrappingMode::NoWrap) {
		return std::min(vrow, visual_nlines());
	}
	
	auto pos = cx()->line_index_map.visual_to_logical(vrow);
	return pos.lrow;
}

RowCol AbstractTextEditorApplication::visual_position(SelectionAnchor const &a) const
{
	if (wrappingMode() == WrappingMode::NoWrap) {
		return {a.lrow, a.lcol};
	} else {
		auto pos = cx()->line_index_map.logical_to_visual(a.lrow, a.lcol);
		return RowCol(pos.vrow, pos.vcol);
	}
}

void AbstractTextEditorApplication::_wrap_line(Document::Line *ll, bool force, std::mutex *mutex)
{
	// 通常の1行編集ではset_text()がvisual_linesを空にするためforce不要。
	// 幅・フォント・モード変更では既存断片が残っていても再計算するためforce=trueを使う。
	if (force || ll->sp->meta.visual_lines.empty()) { // 折り返し未処理の場合
		if (wrappingMode() == WrappingMode::NoWrap) {
			ll->sp->meta.visual_lines.resize(1);
		} else {
			ll->sp->meta.visual_lines = wrap_line(*ll, mutex); // 折り返し処理
		}
	}
}

/**
 * @brief 論理行に対応する物理行数を更新する（line_index_map）
 * @param lrow 論理行インデックス
 * @param ll 論理行情報
 * @param mutex 排他制御用のmutex（nullptrの場合は排他制御なし）
 */
void AbstractTextEditorApplication::_update_line_index_map(row_index_t lrow, Document::Line *ll, std::mutex *mutex)
{
	// ValueItemには折り返し数だけでなく各断片のコードポイント数も入れる。
	// これにより行番号だけでなく、論理列と表示列も相互変換できる。
	std::vector<uint32_t> col_list;
	for (Document::Line const &vl : ll->sp->meta.visual_lines) {
		col_list.push_back(vl.sp->meta.logical_col_len);
	}

	if (mutex) mutex->lock();
	cx()->line_index_map.update(lrow, col_list); // 論理行に対応する物理行数を更新
	if (mutex) mutex->unlock();	
}

/**
 * @brief 論理行を更新する
 * @param lrow 論理行番号
 * @param text 更新するテキスト（nulloptの場合はテキストを更新しない）
 * @param force 強制的に折り返し処理を行うかどうか
 * @param mutex 排他制御用のmutex（nullptrの場合は排他制御なし）
 * @return 物理行数が変化したかどうか
 */
bool AbstractTextEditorApplication::_update_line(row_index_t lrow, std::optional<std::vector<char>> text, bool force, std::mutex *mutex)
{
	bool vline_count_changed = false;
	
	Document *doc = &cx()->engine->document;
	std::vector<Document::Line> *llines = &doc->logical_lines;
	if (lrow >= 0 && lrow < llines->size()) {
		Document::Line *ll = &(*llines)[lrow];
		
		const size_t nvlines = ll->sp->meta.visual_lines.size(); // 折り返し前の物理行数
		
		if (text) { // テキストが指定されている場合、論理行のテキストを更新する
			// set_text()で解析revisionを進め、その行の派生データだけを失効させる。
			// 後続論理行の折り返し結果は内容に依存しないので再計算しない。
			ll->set_text(*text);
			ll->clear_visual_lines();
			ll->sp->meta.detail.reset();
		}
		
		// visual_linesとLineIndexMapは必ずこの順で同じ処理内から更新する。
		// 片方だけが新しい状態になる期間を関数外へ持ち出さない。
		_wrap_line(ll, force, mutex); // 折り返し処理を行う
		_update_line_index_map(lrow, ll, mutex); // 論理行に対応する物理行数を更新
		
		vline_count_changed = (nvlines != ll->sp->meta.visual_lines.size()); // 折り返し後の物理行数が変化したかどうか
	}
	
	if (vline_count_changed) { // 物理行数が変化した場合、物理行数キャッシュを無効化する
		invalidate_nlines_cache();
	}
	return vline_count_changed;
}

/**
 * @brief 論理行番号に対応する物理行情報を更新する
 * @param lrow 論理行番号
 * @param force 強制的に折り返し処理を行うかどうか
 * @return 物理行数が変化したかどうか
 */
bool AbstractTextEditorApplication::update_visual_line(row_index_t lrow, bool force)
{
	std::vector<Document::Line> *llines = &document()->logical_lines;
	if (lrow >= 0 && lrow < llines->size()) {
		_update_line(lrow, std::nullopt, force, nullptr);
		return true;
	}
	return false;
}

/**
 * @brief 全論理行の折り返し結果とLineIndexMapを再構築する
 *
 * 幅などの条件が変わっていなければ何もしない。必要な場合は各論理行の
 * 折り返しを並列計算し、完了後にLineIndexMapを論理行順で構築する。
 */
void AbstractTextEditorApplication::update_visual_lines_all()
{
	TextEditorContext *cx = this->cx();
	// 高さだけのresizeなど、折り返し条件が変わっていない要求はここで終了する。
	// wrapping時は外部から論理行が差し替わった場合も検出するため行数を照合する。
	if (!m->full_wrap_update_needed &&
		(m->wrapping_mode == WrappingMode::NoWrap ||
		 cx->line_index_map.total_logical_row_count() == document()->logical_lines.size())) {
		updateScrollBarRange();
		return;
	}

	// 全再計算では古い索引を段階的に更新せず、一度空にして同じ順序で再構築する。
	cx->line_index_map.clear();
	
	if (m->wrapping_mode != WrappingMode::NoWrap) {
		std::vector<Document::Line> *llines = &cx->engine->document.logical_lines;
		
		if (0) { // シングルスレッド
			for (row_index_t lrow = 0; lrow < (row_index_t)llines->size(); lrow++) {
				update_visual_line(lrow, true);
			}
		} else {
			{ // 並列処理で折り返し処理を行う
				// 各スレッドは別々の論理行だけを書き換える。共有されるフォント幅
				// キャッシュは_parseLine()内でmutex保護される。
				constexpr int nthreads = 8;
				std::mutex mutex;
				std::vector<std::thread> thread(nthreads);
				std::atomic<row_index_t> index = 0;
				const row_index_t nlines = (row_index_t)llines->size();
				for (int i = 0; i < nthreads; i++) {
					thread[i] = std::thread([&](){
						while (1) {
							row_index_t lrow = index++;
							if (lrow >= nlines) break;
							Document::Line *ll = &(*llines)[lrow];
							_wrap_line(ll, true, &mutex);
						}
					});
				}
				for (int i = 0; i < nthreads; i++) {
					thread[i].join();
				}
			}
			// 並列計算完了後、論理行順に索引を構築する。LineIndexMap自体は
			// スレッドセーフである必要がなく、部分的な状態を描画側へ公開しない。
			for (size_t lrow = 0; lrow < llines->size(); lrow++) {
				std::vector<Document::Line> *llines = &document()->logical_lines;
				Document::Line *ll = &(*llines)[lrow];
				// 論理行に対応する物理行数を更新
				_update_line_index_map(lrow, ll, nullptr);
			}
			// 物理行数キャッシュを無効化する
			invalidate_nlines_cache();
		}
	} else {
		invalidate_nlines_cache();
	}
	Q_ASSERT(cx->line_index_map.validate());
	if (m->wrapping_mode != WrappingMode::NoWrap) {
		Q_ASSERT(cx->line_index_map.total_logical_row_count() == document()->logical_lines.size());
	}
	m->full_wrap_update_needed = false;
	
	updateScrollBarRange();
}

/**
 * @brief 論理行を更新する（テキストを指定して更新する）
 * @param lrow 論理行番号
 * @param vec 更新する文字列（Characterの配列）
 * @return 物理行数が変化したかどうか
 */
bool AbstractTextEditorApplication::commit_line(row_index_t lrow, CharBuffer const &vec)
{
	std::vector<Document::Line> *llines = documentLinesForWrite();
	if (!llines) return false;

	// Characterの配列をUTF-8に変換する
	std::vector<char> ba;
	if (!vec.empty()){
		std::vector<char32_t> v;
		v.reserve(vec.size());
		for (Character const &c : *vec.vec) {
			v.push_back(c.unicode);
		}
		utf32 u32(&v[0], v.size());
		u32.to_utf8([&](char c, int pos){
			(void)pos;
			ba.push_back(c);
			return true;
		});
	}

	// 論理行番号が範囲外の場合は、新しい論理行を追加する
	if (lrow == llines->size()) {
		Document::Line newline;
		newline.sp->meta.type = Document::LineType::Normal;
		llines->push_back(newline);
	}
	
	invalidate_logical_row_info(lrow);
	
	return _update_line(lrow, ba, false, nullptr);
}

void AbstractTextEditorApplication::layoutEditor()
{
	// makeBuffer();
	editor_cx->viewport_org_x_cols = leftMargin_();
	editor_cx->viewport_org_y_rows = 0;
	editor_cx->viewport_width_px = client_width_px() - cx()->viewport_org_x_cols * textFontMetrics().basis_char_width();
	editor_cx->viewport_height_rows = client_height_px() / line_height_px();
}

void AbstractTextEditorApplication::initEditor()
{
	editor_cx = std::make_shared<TextEditorContext>();
	layoutEditor();
}

bool AbstractTextEditorApplication::isLineNumberVisible() const
{
	return m->show_line_number;
}

int AbstractTextEditorApplication::charWidth(uint32_t c)
{
	return UnicodeWidth::width(UnicodeWidth::type(c));
}

int AbstractTextEditorApplication::client_width_px() const
{
	return m->client_width_px;
}

int AbstractTextEditorApplication::client_height_px() const
{
	return m->client_height_px;
}

void AbstractTextEditorApplication::set_client_size(int w, int h, bool update_layout)
{
	m->client_width_px = w;
	m->client_height_px = h;
	if (update_layout) {
		layoutEditor();
	}
}

void AbstractTextEditorApplication::setContentWidth(int w)
{
	// 同じ幅なら折り返し結果は有効。高さ変更だけのresizeで全行を再計算しない。
	if (m->content_width_px == w) return;
	m->content_width_px = w;
	m->full_wrap_update_needed = true;
}

bool AbstractTextEditorApplication::isPaintingSuppressed() const
{
	return m->is_painting_suppressed;
}

void AbstractTextEditorApplication::setPaintingSuppressed(bool f)
{
	m->is_painting_suppressed = f;
}

std::vector<Document::Line> *AbstractTextEditorApplication::documentLinesForWrite(bool check_readonly)
{
	if (check_readonly && is_read_only()) return nullptr;
	return &document()->logical_lines;
}

void AbstractTextEditorApplication::setDocument(std::vector<Document::Line> const *source)
{
	std::vector<Document::Line> *lines = documentLinesForWrite(false);
	if (!lines) return;
	
	if (source) {
		// Lineはshared_ptr<D>を持つため、これはLine内容の浅いコピーになる。
		*lines = *source;
	} else {
		lines->clear();
	}
	// 文書全体の置換では既存のLineIndexMapを流用できない。
	m->full_wrap_update_needed = true;
	update_visual_lines_all();
}

void AbstractTextEditorApplication::insert_line(row_index_t lrow)
{
	std::vector<Document::Line> *llines = documentLinesForWrite();
	if (!llines) return;

	llines->insert(llines->begin() + lrow, Document::Line::NormalEmptyLine());
	// LineIndexMapのキーは論理行の位置なので、同じ位置へ空エントリを挿入し、
	// 後続キーもDocumentと一緒にシフトさせる。commit_line()が直後に実値へ更新する。
	cx()->line_index_map.insert(lrow, {});

	invalidate_nlines_cache();
}

CharBuffer AbstractTextEditorApplication::parseLogicalLine(TextEditorContext const *cx, row_index_t lrow) const
{
	std::vector<Document::Line> const &lines = cx->engine->document.logical_lines;
	if (lrow >= 0 && lrow < lines.size()) {
		Document::Line const *line = &lines[lrow];
		return _parseLine(line);
	}
	return {};
}

bool AbstractTextEditorApplication::isCurrentLineWritable() const
{
	if (is_read_only()) return false;

	row_index_t vrow = current_visual_row();
	if (vrow >= 0 && vrow < visual_nlines()) {
		if (visual_line(vrow)->sp->meta.type != Document::LineType::Invalid) {
			return true;
		}
	}
	return false;
}

int AbstractTextEditorApplication::editor_viewport_width_px() const
{
	return client_width_px() - editor_cx->viewport_org_x_cols * m->font.fixed.basis_char_width();
}

int AbstractTextEditorApplication::editor_viewport_height() const
{
	return cx()->viewport_height_rows;
}

void AbstractTextEditorApplication::initEngine(std::shared_ptr<TextEditorContext> const &cx)
{
	cx->engine = std::make_shared<TextEditorEngine>();
}

TextEditorContext *AbstractTextEditorApplication::cx()
{
	if (!editor_cx->engine) {
		initEngine(editor_cx);
	}
	return editor_cx.get();
}

const TextEditorContext *AbstractTextEditorApplication::cx() const
{
	return const_cast<AbstractTextEditorApplication *>(this)->cx();
}

TextEditorEngine_sp AbstractTextEditorApplication::engine() const
{
	Q_ASSERT(cx()->engine);
	return cx()->engine;
}

void AbstractTextEditorApplication::new_document()
{
	// Document、座標変換索引、行数キャッシュを同時に初期化する。
	// 空文書でも編集可能な論理行を必ず1行持たせる。
	cx()->engine->document = {};
	cx()->cache = {};
	cx()->line_index_map.clear();
	m->full_wrap_update_needed = false;
	insert_line(0);
	commit_line(0, {});
	setCursorPos({});
	updateVisibility({});
}

void AbstractTextEditorApplication::setTextEditorEngine(TextEditorEngine_sp const &e)
{
	cx()->engine = e;
	new_document();
}

void AbstractTextEditorApplication::clear()
{
	setDocument(nullptr);
}

void AbstractTextEditorApplication::writeNewLine()
{
	if (is_read_only()) return;

	row_index_t vrow = current_visual_row();
	row_index_t lrow = current_logical_row();
	col_index_t lcol = current_logical_col();
	
	invalidate_visual_row_info(vrow);
	
	CharBuffer curr_line;
	CharBuffer next_line;
	
	curr_line = parseLogicalLine(cx(), lrow);
	
	// 行を分割
	next_line.insert(next_line.end(), curr_line.begin() + lcol, curr_line.end());
	curr_line.resize(lcol);
	curr_line.emplace_back('\n');
	
	// 現在の行を確定
	commit_line(lrow, curr_line);

	// 次の行を挿入
	lrow++;
	insert_line(lrow);
	commit_line(lrow, next_line);

	vrow++;
	set_current_visual_row(vrow);

	setCursorCol(0);
	updateVisibility({});
}

bool AbstractTextEditorApplication::openFile(QString const &path)
{
	document()->logical_lines.clear();
	QFile file(path);
	if (!file.open(QFile::ReadOnly)) return false;
	
	document()->all = file.readAll();
	std::vector<Document::varline_t> lines;
	char const *begin = document()->all.data();
	char const *end = begin + document()->all.size();
	char const *left = begin;
	char const *right = begin;
	while (1) {
		int c = -1;
		if (right < end) {
			c = (unsigned char)*right++;
		}
		if (c == '\r' || c == '\n' || c == -1) {
			if (c == '\r' && right < end && *right == '\n') {
				right++;
			}
			std::string_view line(left, right - left);
			lines.emplace_back(line);
			if (c == -1) break;
			left = right;
		}
	}
	for (size_t i = 0; i < lines.size(); i++) {
		assert(std::holds_alternative<std::string_view>(lines[i]));
		std::string_view sv = std::get<std::string_view>(lines[i]);
		auto line = Document::Line::View(sv);
		line.sp->meta.type = Document::LineType::Normal;
		document()->logical_lines.push_back(line);
	}
	document()->raw_lines = std::move(lines);

	if (document()->logical_lines.empty()) {
		Document::Line line;
		line.sp->meta.type = Document::LineType::Normal;
		document()->logical_lines.push_back(line);
	}

	update_visual_lines_all();
	
	scrollToTop();

	setRecentlyUsedPath(path);
	return true;
}

void AbstractTextEditorApplication::saveFile(QString const &path)
{
	QFile file(path);
	if (file.open(QFile::WriteOnly)) {
		save([&file](char const *p, size_t n){
			return file.write(p, n) == n;
		});
	}
}

void AbstractTextEditorApplication::pressEnter()
{
	deleteIfSelected();
	writeNewLine();
}

void AbstractTextEditorApplication::pressEscape()
{
	if (isTerminalMode()) {
		m->escape_sequence.write(0x1b);
		return;
	}

	updateVisibility({false, false, false});
}

AbstractTextEditorApplication::State AbstractTextEditorApplication::state() const
{
	return m->state;
}

Document *AbstractTextEditorApplication::document()
{
	return &engine()->document;
}

Document const *AbstractTextEditorApplication::document() const
{
	return &engine()->document;
}

void AbstractTextEditorApplication::setLineMargin(int n)
{
	m->line_margin = n;
}

void AbstractTextEditorApplication::ensureCurrentLineVisible()
{
	int margin = (cx()->viewport_height_rows >= m->line_margin * 2) ? m->line_margin : 0;
	int pos = scroll_vert_pos_px();
	int top = current_visual_row() - margin;
	int bottom = current_visual_row() + 1 - editor_viewport_height() + margin;
	pos = std::min(pos, top);
	pos = std::max(pos, bottom);
	pos = std::max(pos, 0);
	if (scroll_vert_pos_px() != pos) {
		set_scroll_vert_pos_px(pos);
	}
}

bool AbstractTextEditorApplication::isWidthFixed() const
{
	return (wrappingMode() != WrappingMode::NoWrap);
}

void AbstractTextEditorApplication::savePos()
{
	TextEditorContext *p = editor_cx.get();
	if (p) {
		p->saved_row = current_visual_row();
		p->saved_col = current_visual_col();
		p->saved_col_hint = p->current_visual_col_hint;
	}
}

void AbstractTextEditorApplication::restorePos()
{
	TextEditorContext *p = editor_cx.get();
	if (p) {
		set_current_visual_row(p->saved_row);
		set_current_visual_col(p->saved_col);
		p->current_visual_col_hint = p->saved_col_hint;
	}
}

bool AbstractTextEditorApplication::hasSelection() const
{
	return !selection_end();
}

void AbstractTextEditorApplication::updateSelectionAnchor1(bool auto_scroll)
{
	if (isShiftModifierPressed()) {
		if (!selection_end()) {
			setSelectionAnchor(true, true, auto_scroll);
			sync_selection();
		}
	} else if (selection_end()) {
		// 選択中でShiftが押されていなければ選択解除
		setSelectionAnchor(false, false, auto_scroll);
	}
}

void AbstractTextEditorApplication::updateSelectionAnchor2(bool auto_scroll)
{
	if (selection_end()) {
		// 選択中なら、現在位置で更新
		setSelectionAnchor(true, true, auto_scroll);
	}
}

void AbstractTextEditorApplication::setFixedFont(const QFont &font)
{
	m->font.fixed.set_font(font);
	// タブ幅や基準セル幅が変わるため、全LinePropertyの計測結果を世代で失効させる。
	m->metrics_revision++;
	m->full_wrap_update_needed = true;
}

void AbstractTextEditorApplication::setTextFont(const QFont &font)
{
	m->font.text.set_font(font);
	// 本文の実ピクセル幅が変わるため、計測と折り返しの両方をやり直す。
	m->metrics_revision++;
	m->full_wrap_update_needed = true;
}

AbstractTextEditorApplication::Font const &AbstractTextEditorApplication::fixedFontMetrics() const
{
	return m->font.fixed;
}

AbstractTextEditorApplication::Font const &AbstractTextEditorApplication::textFontMetrics() const
{
	return m->font.text;
}

void AbstractTextEditorApplication::set_line_margin_px(int top, int bottom)
{
	m->top_margin_px = top;
	m->bottom_margin_px = bottom;
}

int AbstractTextEditorApplication::line_height_px() const
{
	int h = fixedFontMetrics().basis_char_height() + m->top_margin_px + m->bottom_margin_px;
	return h > 0 ? h : 16;
}

int AbstractTextEditorApplication::line_baseline_px() const
{
	int h = line_height_px() - m->bottom_margin_px - fixedFontMetrics().descent_;
	return h > 0 ? h : 16;
}

void AbstractTextEditorApplication::setCursorRow(row_index_t vrow, bool auto_scroll, bool by_mouse)
{
	if (vrow < 0) {
		vrow = 0;
	} else {
		const row_index_t n = visual_nlines();
		if (vrow >= n) {
			vrow = (n > 0) ? (n - 1) : 0;
		}
	}
	
	if (current_visual_row() == vrow) return;

	updateSelectionAnchor1(false);

	set_current_visual_row(vrow);

	updateSelectionAnchor2(auto_scroll);

	m->cursor_moved_by_mouse = by_mouse;
}

void AbstractTextEditorApplication::_set_cursor_col(col_index_t vcol, bool auto_scroll, bool by_mouse)
{
	if (current_visual_col() == vcol) {
		cx()->current_visual_col_hint = vcol;
		return;
	}

	updateSelectionAnchor1(false);

	set_current_visual_col(vcol);
	cx()->current_visual_col_hint = vcol;

	updateSelectionAnchor2(auto_scroll);

	m->cursor_moved_by_mouse = by_mouse;
}

void AbstractTextEditorApplication::setCursorCol(col_index_t vcol)
{
	_set_cursor_col(vcol, true, false);
	auto pair = currentPixelX(); // カーソルのピクセル位置を更新する
	cx()->current_visual_x_px = pair.first;
	cx()->current_absolute_x_px = pair.second;
}

void AbstractTextEditorApplication::setCursorPos(const RowCol &vpos)
{
	setCursorRow(vpos.row, false);
	setCursorCol(vpos.col);
}

void AbstractTextEditorApplication::setCursorPosByMouse(RowCol vpos, QPoint pt)
{
	setCursorRow(vpos.row, false, true);
	_set_cursor_col(vpos.col, false, true);
	cx()->current_visual_x_px = pt.x(); // マウスでクリックした位置にカーソルを移動した場合は、現在のピクセル位置をマウスの位置に合わせる
}

int AbstractTextEditorApplication::nextTabStop(const TextEditorContext *cx, int x)
{
	x += cx->tab_indent_size;
	x -= x % cx->tab_indent_size;
	return x;
}

void AbstractTextEditorApplication::edit_selection(EditOperation op, CharBuffer *clip_text_out)
{
	if (clip_text_out) {
		clip_text_out->clear();
	}

	auto AppendClipText = [&clip_text_out](CharBuffer &chars, size_t begin, size_t end){
		// 空行では&chars[0]を作れない。iteratorの範囲も空ならinsert自体を省略する。
		if (clip_text_out && begin < end) {
			clip_text_out->insert(clip_text_out->end(), chars.begin() + begin, chars.begin() + end);
		}
	};
	
	if (is_read_only() && op == EditOperation::Cut) { // 読み取り専用モードでは切り取りはできないのでコピーに変更
		op = EditOperation::Copy;
	}

	SelectionAnchor a = selection_start();
	SelectionAnchor b = selection_end();
	// 選択範囲がない場合は何もしない
	if (!a) return;
	if (!b) return;
	if (a == b) return;
	// 選択範囲の開始位置と終了位置を入れ替える
	if (a > b) {
		std::swap(a, b);
	}

	std::vector<Document::Line> const *llines = &document()->logical_lines;
	// SelectionAnchorはイベント処理や座標変換から来るが、編集処理の境界でも検証する。
	// 不正な行は拒否し、列は実際のコードポイント数へ丸めてiterator範囲を保証する。
	if (a.lrow < 0 || b.lrow < 0 ||
		a.lrow >= (row_index_t)llines->size() || b.lrow >= (row_index_t)llines->size()) {
		return;
	}
	auto ClampColumn = [&](SelectionAnchor *anchor){
		CharBuffer chars = parseLogicalLine(cx(), anchor->lrow);
		if (anchor->lcol < 0) {
			anchor->lcol = 0;
		} else if ((size_t)anchor->lcol > chars.size()) {
			anchor->lcol = (col_index_t)chars.size();
		}
	};
	ClampColumn(&a);
	ClampColumn(&b);
	if (a == b) return;

	auto UpdateVisibility = [&](){
		updateVisibility({false, false, false});
	};

	CharBuffer cliptext;

	bool cut = false;
	if (op == EditOperation::Cut) {
		invalidate_visual_row_info(lrow_to_vrow(a.lrow));
		cut = true;
	}

	row_index_t end_lrow = std::min(b.lrow + 1, (row_index_t)llines->size());
	if (a.lrow == b.lrow) { // 選択範囲が1行のみの場合
		if (a.lcol < b.lcol) {
			CharBuffer chars = parseLogicalLine(cx(), a.lrow);
			AppendClipText(chars, a.lcol, b.lcol);
			if (cut) { // 切り取りの場合は、選択範囲の文字を削除して行を更新
				chars.erase(chars.begin() + a.lcol, chars.begin() + b.lcol);
				commit_line(a.lrow, chars);
			}
		}
	} else {
		std::vector<row_index_t> delete_list;
		SelectionAnchor curr = a;
		// 選択範囲の論理行を順に処理
		while (curr.lrow < end_lrow) {
			CharBuffer chars = parseLogicalLine(cx(), curr.lrow);
			size_t begin = 0;
			size_t end = chars.size();
			bool entire = false;
			if (curr.lrow == a.lrow) {
				begin = std::min((size_t)a.lcol, end);
			} else if (curr.lrow == b.lrow) {
				end = std::min((size_t)b.lcol, end);
			} else {
				entire = true;
			}
			AppendClipText(chars, begin, end);
			if (cut) { // 切り取りの場合は、選択範囲の文字を削除して行を更新
				if (entire) { // 論理行全体が選択されている場合は、行を削除する
					delete_list.push_back(curr.lrow); // 後ろから削除するため削除リストに登録
				} else { // 論理行の一部が選択されている場合は、選択範囲の文字を削除して行を更新する
					chars.erase(chars.begin() + begin, chars.begin() + end);
					commit_line(curr.lrow, chars);
				}
			}
			curr.lrow++;
		}
		if (cut) { // 切り取りの場合
			// 削除リストにある論理行を削除する
			for (auto it = delete_list.rbegin(); it != delete_list.rend(); it++) {
				delete_line(*it);
			}
			CharBuffer chars = parseLogicalLine(cx(), a.lrow);
			if (!chars.empty()) {
				char32_t c = chars.back().unicode;
				if (c != '\n' && c != '\r') { // 最後の文字が改行でない場合は、次の行を結合する
					CharBuffer next = parseLogicalLine(cx(), a.lrow + 1);
					if (!next.empty()) {
						// 次の行の文字を現在の行の末尾に追加して、現在の行を更新
						chars.insert(chars.end(), next.begin(), next.end());
						commit_line(a.lrow, chars);
						// 次の行を削除する
						delete_line(a.lrow + 1);
					}
				}
			}
		}
	}
	if (cut) { // 切り取りの場合
		clear_selection(); // 選択範囲をクリア
		setCursorPos(visual_position(a)); // カーソルを選択範囲の開始位置に移動
	}

	UpdateVisibility();
}

void AbstractTextEditorApplication::_edit_op(EditOperation op)
{
	CharBuffer cutbuf;
	edit_selection(op, &cutbuf);
	if (cutbuf.empty()) return;

	std::vector<char32_t > c32buf;
	c32buf.reserve(cutbuf.size());
	for (Character const &c : cutbuf) {
		c32buf.push_back(c.unicode);
	}

	std::vector<char16_t> u16buf;
	u16buf.reserve(1024);
	utf32(c32buf.data(), c32buf.size()).to_utf16([&](uint16_t c){
		u16buf.push_back(c);
		return true;
	});
	if (!u16buf.empty()) {
		QString s = QString::fromUtf16(&u16buf[0], (int)u16buf.size());
		qApp->clipboard()->setText(s);
	}
}

bool AbstractTextEditorApplication::deleteIfSelected()
{
	if (selection_end() && selection_start()) {
		if (selection_end() != selection_start()) {
			edit_selection(EditOperation::Cut, nullptr);
			return true;
		}
	}
	return false;
}

void AbstractTextEditorApplication::delete_line(row_index_t lrow)
{
	std::vector<Document::Line> *llines = &document()->logical_lines;
	if (lrow < llines->size()) {
		llines->erase(llines->begin() + lrow);
	}
	// Documentと同じ位置を削除し、後続論理行のキーを詰める。
	// 折り返しキャッシュは各Lineに属するため、後続行の再計算は不要。
	cx()->line_index_map.erase(lrow);
	invalidate_nlines_cache();
}

void AbstractTextEditorApplication::doDelete()
{
	if (is_read_only()) return;
	if (isTerminalMode()) return;

	if (deleteIfSelected()) {
		return;
	}
	
	Document *doc = document();
	
	col_index_t lrow = current_logical_row();
	col_index_t lcol = current_logical_col();
	CharBuffer chars = parseLogicalLine(cx(), lrow);
	bool delete_nl = false; // 削除した文字が改行コードであるかどうか
	char32_t c = -1;
	if (lcol >= 0 && lcol < (int)chars.size()) {
		c = (chars)[lcol].unicode;
	}
	if (c == '\n' || c == '\r' || c == -1) {
		if (c != -1) {
			chars.erase(chars.begin() + lcol);
			if (c == '\r' && lcol < (int)chars.size() && (chars)[lcol].unicode == '\n') {
				chars.erase(chars.begin() + lcol);
			}
		}
		delete_nl = true;
		if (lcol == (int)chars.size()) { // カーソルが行末にある場合は、次の行を結合する
			row_index_t next_lrow = lrow + 1;
			CharBuffer next = parseLogicalLine(cx(), next_lrow);
			chars.insert(chars.end(), next.begin(), next.end());
			if (next_lrow < logical_nlines()) {
				delete_line(next_lrow);
			}
		}
	} else {
		chars.erase(chars.begin() + lcol);
	}

	row_index_t vrow = lrow_to_vrow(lrow);
	col_index_t vcol = current_visual_col();
	
	row_index_t invalidate_vrow = -1; // 物理行情報を無効化する論理行番号
	
	if (commit_line(lrow, chars)) {
		// 折り返し後の物理行数が変化した場合は、次の行以降の物理行情報を無効化する
		invalidate_vrow = vrow + 1;
	}
	
	if (delete_nl) {
		// 削除した文字が改行コードなら、現在行以降の物理行情報を無効化する
		invalidate_vrow = vrow;
	}
	
	if (invalidate_vrow != -1) {
		invalidate_visual_row_info(invalidate_vrow);
	}
	
	vcol = lcol; // 論理行から物理行を再計算
	std::vector<Document::Line> *llines = &doc->logical_lines;
	Document::Line const &line = (*llines)[lrow];
	for (size_t i = 0; i < line.sp->meta.visual_lines.size(); i++) {
		CharBuffer chars = _parseLine(&line.sp->meta.visual_lines[i]);
		if (vcol <= chars.size()) break;
		vcol -= chars.size();
		vrow++;
	}
	if (visual_nlines() > 0 && vrow >= visual_nlines()) {
		vrow = visual_nlines() - 1;
	}
	setCursorPos({vrow, vcol});

	updateVisibility({});
}

void AbstractTextEditorApplication::doBackspace()
{
	if (is_read_only()) return;
	if (isTerminalMode()) return;

	if (deleteIfSelected()) {
		return ;
	}

	if (current_visual_row() > 0 || current_visual_col() > 0) {
		setPaintingSuppressed(true);
		moveCursorLeft();
		doDelete();
		setPaintingSuppressed(false);
		updateVisibility({});
	}
}

int AbstractTextEditorApplication::calcColumnToIndex(int column)
{
	int index = 0;
	if (column > 0) {
		if (Document::Line const *line = currentLine()) {
			std::string_view text = line->text();
			int col = 0;
			int len = text.size();
			if (len > 0) {
				char const *src = text.data();
				utf8 u8(src, len);
				while (1) {
					uint32_t c = u8.next();
					int n = 0;
					if (c == '\r' || c == '\n' || c == 0) {
						break;
					}
					if (c == '\t') {
						int z = nextTabStop(cx(), col);
						n = z - col;
					} else {
						n = charWidth(c);
					}
					col += n;
					index++;
					if (col >= column) {
						break;
					}
				}
			}
		}
	}
	return index;
}

int AbstractTextEditorApplication::scrollBottomLimit() const
{
	return logical_nlines() - editor_viewport_height() / 2;
}

int AbstractTextEditorApplication::scrollBottomLimit2() const
{
	return logical_nlines() - editor_viewport_height();
}

void AbstractTextEditorApplication::moveCursorOut()
{
	setCursorRow(-1);
}

void AbstractTextEditorApplication::moveCursorHome(bool consider_indent)
{
	col_index_t vcol = 0;

	if (consider_indent) { // 行頭の空白を飛ばす
		row_index_t vrow = current_visual_row();
		if (vrow == lrow_to_vrow(current_logical_row())) { // 論理行の先頭なら
			CharBuffer const *vline = parseCurrentLine();
			if (vline) {
				const col_index_t ncols = vline->size();
				col_index_t indent_vcol = 0;
				while (indent_vcol < ncols) {
					char32_t c = (*vline)[indent_vcol].unicode;
					if (c == ' ' || c == '\t') {
						indent_vcol++;
					} else {
						break;
					}
				}
				col_index_t curr_vcol = current_visual_col();
				vcol = (curr_vcol > 0 && curr_vcol <= indent_vcol) ? 0 : indent_vcol; // カーソルがインデントの範囲内なら行頭へ、そうでなければインデントの先頭へ
			}
		}
	}
	
	setCursorCol(vcol);
	updateVisibility({});
}

void AbstractTextEditorApplication::moveCursorEnd()
{
	CharBuffer const *vline = parseCurrentLine();
	if (!vline) return;
	
	col_index_t col = vline->size();
	
	while (col > 0) { // 行末の改行コードを飛ばす
		char32_t c = (*vline)[col - 1].unicode;
		if (c == '\r' || c == '\n') {
			col--;
		} else {
			break;
		}
	}
	
	setCursorCol(col);
	updateVisibility({});
}

void AbstractTextEditorApplication::scrollUp()
{
	if (scroll_vert_pos_px() > 0) {
		set_scroll_vert_pos_px(scroll_vert_pos_px() - 1);
		updateVisibility({false, false, true});
	}
}

void AbstractTextEditorApplication::scrollDown()
{
	int limit = scrollBottomLimit();
	if (scroll_vert_pos_px() < limit) {
		set_scroll_vert_pos_px(scroll_vert_pos_px() + 1);
		updateVisibility({false, false, true});
	}
}

void AbstractTextEditorApplication::moveCursorUp()
{
	row_index_t vrow = current_visual_row();
	if (vrow > 0) {
		vrow--;
	}
	setCursorRow(vrow); // カーソルを1行上へ
	updateVisibility({true, false, true});
}

void AbstractTextEditorApplication::moveCursorDown()
{
	row_index_t vrow = current_visual_row();
	if (vrow + 1 < visual_nlines()) {
		vrow++;
	}
	setCursorRow(vrow); // カーソルを1行下へ
	updateVisibility({true, false, true});
}

void AbstractTextEditorApplication::scrollToTop()
{
	setCursorRow(0);
	setCursorCol(0);
	set_scroll_vert_pos_px(0);
	updateVisibility({true, false, true});
}

void AbstractTextEditorApplication::moveCursorLeft()
{
	if (!isShiftModifierPressed() && selection_end() && selection_start()) { // 選択領域があったら
		if (selection_end() != selection_start()) {
			SelectionAnchor a = std::min(selection_end(), selection_start()); // 選択範囲の先頭位置
			clear_selection();
			setCursorPos(visual_position(a)); // 選択範囲の先頭位置にカーソルを移動
			updateVisibility({});
			return;
		}
	}
	
	col_index_t vcol = current_visual_col();
	if (vcol == 0) { // 行頭なら
		row_index_t vrow = current_visual_row();
		if (vrow > 0) {
			const auto prev_pos = query_logical_for_visual_row(vrow - 1);
			const auto curr_pos = query_logical_for_visual_row(vrow);
			setCursorRow(vrow - 1); // 上へ移動
			moveCursorEnd(); // 行末へ移動
			if (prev_pos.lrow == curr_pos.lrow) { // 同じ論理行の続きなら
				moveCursorLeft(); // 左へ移動
			}
		}
		return;
	}

	setCursorCol(current_visual_col() - 1);
	updateVisibility({});
}

void AbstractTextEditorApplication::moveCursorRight()
{
	if (!isShiftModifierPressed() && selection_end() && selection_start()) { // 選択領域があったら
		if (selection_end() != selection_start()) {
			SelectionAnchor a = std::max(selection_end(), selection_start()); // 選択範囲の末尾位置
			clear_selection();
			setCursorPos(visual_position(a)); // 選択範囲の先頭位置にカーソルを移動
			updateVisibility({});
			return;
		}
	}

	auto MoveToNextRow = [this](){
		int next_vrow = current_visual_row() + 1;
		if (next_vrow < visual_nlines()) {
			setCursorRow(next_vrow, false);
			moveCursorHome(false); // 行頭へ移動
			return true;
		}
		return false;
	};
	
	auto MoveColumn = [this](col_index_t vcol){
		if (vcol != current_visual_col()) {
			setCursorCol(vcol);
			updateVisibility({});
			return true;
		}
		return false;
	};
	
	const auto curr_pos = query_logical_for_visual_row(current_visual_row());
	const auto next_pos = query_logical_for_visual_row(current_visual_row() + 1);
	
	CharBuffer const *vline = parseCurrentLine();
	if (!vline) return;
	
	col_index_t vcol = current_visual_col();
	
	char32_t c = -1;
	if (vcol < vline->size()) {
		c = (*vline)[vcol].unicode;
	}
	if (c == '\r' || c == '\n' || c == (char32_t)-1) {
		MoveToNextRow(); // 次の行の先頭へ移動
		return;
	}
	
	vcol++;
	if (vcol > current_visual_col()) {
		if (curr_pos.lrow == next_pos.lrow) { // 同じ論理行の続きなら
			const size_t len = next_pos.lcol - curr_pos.lcol; // 物理行の長さ
			if (vcol >= len) { // 行末
				if (MoveToNextRow()) return;
			}
		}
		if (MoveColumn(vcol)) return;
	}
}

void AbstractTextEditorApplication::movePageUp()
{
	int step = editor_viewport_height();
	setCursorRow(current_visual_row() - step);
	set_scroll_vert_pos_px(scroll_vert_pos_px() - step);
	if (current_visual_row() < 0) {
		set_current_visual_row(0);
	}
	if (scroll_vert_pos_px() < 0) {
		set_scroll_vert_pos_px(0);
	}
	updateVisibility({true, false, true});
}

void AbstractTextEditorApplication::movePageDown()
{
	row_index_t vrow_limit = visual_nlines();
	if (vrow_limit > 0) {
		vrow_limit--;
		int step = editor_viewport_height();
		row_index_t curr_vrow = current_visual_row();
		row_index_t next_vrow = std::min(curr_vrow + step, vrow_limit);
		int scroll_pos = scroll_vert_pos_px() + (next_vrow - curr_vrow);
		scroll_pos = std::min(scroll_pos, scrollBottomLimit());
		setCursorRow(next_vrow);
		set_scroll_vert_pos_px(scroll_pos);
	} else {
		setCursorRow(0);
		set_scroll_vert_pos_px(0);
	}
	updateVisibility({true, false, true});
}

void AbstractTextEditorApplication::update_horz_scroll()
{
	if (isWidthFixed()) return; // 固定幅の場合は水平スクロールはしない

	// 計算はすべてピクセル単位	
	int x = cx()->current_absolute_x_px; // カーソルの絶対位置（行頭基準）
	auto curr_scroll_pos = scroll_horz_pos_px(); // 現在の水平スクロール位置
	auto textarea_width = client_width_px() - linenum_area_width_px(); // テキストエリアの幅
	auto left = textarea_width / 5; // 左端の余白
	auto right = textarea_width * 4 / 5; // 右端の余白
	int pos = curr_scroll_pos;
	if (x - pos > right) {
		pos = x - right;
	} else if (x - pos < left) {
		if (x > left) {
			pos = x - left;
		} else {
			pos = 0;
		}
	}
	if (pos != curr_scroll_pos) {
		set_scroll_horz_pos_px(pos);
		need_to_update_scroll_bar();
	}
}

QString AbstractTextEditorApplication::statusLine() const
{
	QString text = "[%1:%2]";
	text = text.arg(current_visual_row() + 1).arg(current_visual_col() + 1);
	return text;
}

void AbstractTextEditorApplication::paintLineNumbers(std::function<void(int, QString const &, Document::Line const *)> const &draw)
{
	auto Line = [&](row_index_t row)-> Document::Line const & {
		return *visual_line(row);
	};

	int rightpadding = 2;
	int left_margin = editor_cx->viewport_org_x_cols;

	for (int i = 0; i <= editor_cx->viewport_height_rows; i++) {
		row_index_t vrow = editor_cx->scroll_vert_pos_px + i;
		auto LineNumberText = [&](int linenum){
			if (linenum > 0) {
				return QString::asprintf("%*u ", left_margin - rightpadding, linenum);
			}
			return QString();
		};
		QString text;
		Document::Line const *line = nullptr;
		if (vrow < (int)visual_nlines()) {
			if (left_margin > 1) {
				line = &Line(vrow);
				unsigned int linenum = 0;
				if (line->sp->meta.line_number_override >= 0) {
					linenum = line->sp->meta.line_number_override;
				} else {
					auto pos = query_logical_for_visual_row(vrow); // 物理行から論理行番号を取得する
					if (pos.lcol == 0) {
						linenum = pos.lrow + 1;
					}
				}
				if (line->sp->meta.type != Document::LineType::Invalid) {
					text = LineNumberText(linenum);
				}
			}
		} else if (vrow == 0 && visual_nlines() == 0) {
			text = LineNumberText(1);
		}
		int y = editor_cx->viewport_org_y_rows + i;
		draw(y, text, line);
	}
}

bool AbstractTextEditorApplication::isAutoLayout() const
{
	return m->auto_layout;
}

void AbstractTextEditorApplication::setNormalTextEditorMode(bool f)
{
	setTerminalMode(!f);
}

SelectionAnchor AbstractTextEditorApplication::currentAnchor(bool enabled) const
{
	SelectionAnchor a;
	a.lrow = current_logical_row();
	a.lcol = current_logical_col();
	a.enabled = enabled;
	return a;
}

void AbstractTextEditorApplication::set_read_only(bool f)
{
	m->is_read_only = f;
}

bool AbstractTextEditorApplication::is_read_only() const
{
	return m->is_read_only && !m->is_terminal_mode;
}

void AbstractTextEditorApplication::setSelectionAnchor(bool enabled, bool update_anchor, bool auto_scroll)
{
	if (update_anchor) {
		set_selection_end(currentAnchor(enabled));
	} else {
		set_selection_end_enabled(enabled);
	}
	updateVisibility({false, false, auto_scroll});
}

void AbstractTextEditorApplication::editPaste()
{
	if (is_read_only()) return;
	if (isTerminalMode()) return;

	setPaintingSuppressed(true);

	QString str = qApp->clipboard()->text();
	utf16(str.utf16(), str.size()).to_utf32([&](uint32_t c){
		write(c, false);
		return true;
	});

	setPaintingSuppressed(false);
	updateVisibility({});
}

void AbstractTextEditorApplication::edit_copy()
{
	_edit_op(EditOperation::Copy);
}

void AbstractTextEditorApplication::edit_cut()
{
	if (is_read_only()) return;
	if (isTerminalMode()) return;
	_edit_op(EditOperation::Cut);
}

void AbstractTextEditorApplication::setWriteMode(WriteMode wm)
{
	m->write_mode = wm;
}

bool AbstractTextEditorApplication::isInsertMode() const
{
	return m->write_mode == WriteMode::Insert && !isTerminalMode();
}

bool AbstractTextEditorApplication::isOverwriteMode() const
{
	return m->write_mode == WriteMode::Overwrite || isTerminalMode();
}

void AbstractTextEditorApplication::setTerminalMode(bool f)
{
	m->is_terminal_mode = f;
	if (isTerminalMode()) {
		showLineNumber(false, 0);
		setLineMargin(1);
		setWriteMode(WriteMode::Overwrite);
		set_read_only(true);
	}
	layoutEditor();
}

bool AbstractTextEditorApplication::isTerminalMode() const
{
	return m->is_terminal_mode;
}

void AbstractTextEditorApplication::moveToTop()
{
	clear_selection();

	set_current_visual_row(0);
	set_current_visual_col(0);
	cx()->current_visual_col_hint = 0;
	set_scroll_vert_pos_px(0);
	scrollToTop();
	updateVisibility({true, false, true});
}

void AbstractTextEditorApplication::logicalMoveToBottom()
{
	clear_selection();

	row_index_t vrow = visual_nlines();
	if (vrow > 0) {
		vrow--;
	}
	setCursorRow(vrow);
	ensureCurrentLineVisible();
}

void AbstractTextEditorApplication::moveToBottom()
{
	logicalMoveToBottom();

	updateVisibility({true, false, true});
}

void AbstractTextEditorApplication::internalWrite(const ushort *begin, const ushort *end)
{
	if (!isCurrentLineWritable()) return;
	
	deleteIfSelected();
	clearShiftModifier();
	
	Document *doc = document();
	if (doc->logical_lines.empty()) {
		Document::Line line;
		line.sp->meta.type = Document::LineType::Normal;
		doc->logical_lines.push_back(line);
	}

	row_index_t vrow = current_visual_row();
	row_index_t lrow = current_logical_row();
	col_index_t lcol = current_logical_col();

	CharBuffer vec = parseLogicalLine(cx(), lrow);

	auto WriteChar = [&](uint32_t c){
		if (isInsertMode()) {
			assert(lcol >= 0 && lcol <= vec.size());
			vec.insert(vec.begin() + lcol, Character(c));
		} else if (isOverwriteMode()) {
			if (lcol < (int)vec.size()) {
				char32_t d = vec[lcol].unicode;
				if (d == '\n' || d == '\r') { // 行末の改行コードを上書きする場合は、挿入する
					vec.insert(vec.begin() + lcol, Character(c));
				} else {
					vec[lcol] = Character(c);
				}
			} else {
				vec.emplace_back(c);
			}
		}
	};

	ushort const *ptr = begin;
	while (ptr < end) {
		ushort c = *ptr;
		ptr++;
		if (c >= 0xd800 && c < 0xdc00) {
			if (ptr < end) {
				ushort d = *ptr;
				if (d >= 0xdc00 && d < 0xe000) {
					ptr++;
					int u = 0x10000 + (c - 0xd800) * 0x400 + (d - 0xdc00);
					WriteChar(u);
					lcol++;
				}
			}
		} else {
			WriteChar(c);
			lcol++;
		}
	}

	if (commit_line(lrow, vec)) {
		invalidate_visual_row_info(vrow + 1);
	}

	if (wrappingMode() == WrappingMode::NoWrap) {
		setCursorPos({lrow, lcol});
	} else {
		auto [vrow, vcol] = cx()->line_index_map.logical_to_visual(lrow, lcol);
		setCursorPos(RowCol(vrow, vcol));
	}

	updateVisibility({});
}

void AbstractTextEditorApplication::writeCR()
{
	deleteIfSelected();
	
	moveCursorHome(false);
}

void AbstractTextEditorApplication::write(uint32_t c, bool by_keyboard)
{
	if (isTerminalMode()) {
		if (c == '\r') {
			setCursorCol(0);
			updateVisibility({});
			return;
		}
		if (m->cursor_moved_by_mouse) {
			moveToBottom();
		}
		if (c == 0x1b || m->escape_sequence.isActive()) {
			m->escape_sequence.write(c);
			return;
		}
	}

	bool ok = !(isTerminalMode() && by_keyboard);

	if (c < 0x20) {
		if (c == 0x08) {
			if (ok) {
				doBackspace();
			}
		} else if (c == 0x09) {
			if (ok) {
				ushort u = c;
				internalWrite(&u, &u + 1);
			}
		} else if (c == 0x0a) {
			if (ok) {
				pressEnter();
			}
		} else if (c == 0x0d) {
			if (ok) {
				writeCR();
			}
		} else if (c == 0x1b) {
			pressEscape();
		// } else if (c >= 1 && c <= 26) {
		// 	pressLetterWithControl(c);
		}
	} else if (c == 0x7f) {
		if (ok) {
			doDelete();
		}
	} else if (c < 0x10000) {
		if (ok) {
			ushort u = c;
			internalWrite(&u, &u + 1);
		}
	} else if (c >= 0x10000 && c <= 0x10ffff) {
		if (ok) {
			ushort t[2];
			t[0] = (c - 0x10000) / 0x400 + 0xd800;
			t[1] = (c - 0x10000) % 0x400 + 0xdc00;
			internalWrite(t, t + 2);
		}
	} else {
		switch (c) {
		case EscapeCode::Up:
			if (ok) moveCursorUp();
			break;
		case EscapeCode::Down:
			if (ok) moveCursorDown();
			break;
		case EscapeCode::Right:
			if (ok) moveCursorRight();
			break;
		case EscapeCode::Left:
			if (ok) moveCursorLeft();
			break;
		case EscapeCode::Home:
			if (ok) moveCursorHome(true);
			break;
		case EscapeCode::End:
			if (ok) moveCursorEnd();
			break;
		case EscapeCode::PageUp:
			if (ok) movePageUp();
			break;
		case EscapeCode::PageDown:
			if (ok) movePageDown();
			break;
		case EscapeCode::Insert:
			clearShiftModifier();
			break;
		case EscapeCode::Delete:
			clearShiftModifier();
			if (ok) doDelete();
			break;
		}
	}
}

/**
 * @brief 文字列を複数行に分割する（改行コードを保持する）
 * @param begin 文字列の先頭
 * @param size 文字列のサイズ
 * @return 分割された文字列のリスト
 */
static std::vector<std::string_view> split_lines(char const *begin, size_t size)
{
	std::vector<std::string_view> ret;
	char const *end = begin + size;
	char const *ptr = begin;
	char const *left = ptr;
	while (1) {
		int c = 0;
		if (ptr < end) {
			c = (unsigned char)*ptr;
		}
		if (c == '\n' || c == '\r' || c == 0) {
			char const *right = ptr;
			if (c == '\n') {
				ptr++;
			} else if (c == '\r') {
				ptr++;
				if (ptr < end && *ptr == '\n') {
					ptr++;
				}
			}
			if (true) {
				right = ptr; // keep new line
			}
			ret.push_back(std::string_view(left, right - left));
			if (c == 0) break;
			left = ptr;
		} else {
			ptr++;
		}
	}
	return ret;
}

/**
 * @brief 文字列を複数行に分割して、ドキュメントの末尾に追加する
 * @param str 追加する文字列
 */
void AbstractTextEditorApplication::appendBulk(std::string_view const &str)
{
	std::vector<std::string_view> lines = split_lines(str.data(), str.size());

	// 末尾の行が空で、かつその前の行が改行で終わっている場合は、末尾の行を削除する
	if (lines.size() > 1) {
		if (lines[lines.size() - 1].empty()) {
			std::string_view v = lines[lines.size() - 2];
			if (v.size() > 0) {
				char c = v[v.size() - 1];
				if (c == '\n' || c == '\r') {
					lines.pop_back();
				}
			}
		}
	}
	
	Document *doc = document();
	if (!doc->logical_lines.empty()) {
		if (!doc->logical_lines.back().endsWithNewLine()) {
			// 未完の最終論理行へ連結した場合は、その1行だけを再解析・再折り返しする。
			row_index_t lrow = doc->logical_lines.size() - 1;
			doc->logical_lines.back().append_text(str);
			update_visual_line(lrow, false);
			return;
		}
	}
	
	for (std::string_view line : lines) {
		// 末尾追加は各新規行について索引をappendし、既存行の折り返しを保持する。
		Document::Line l(std::vector<char>(line.data(), line.data() + line.size()));
		row_index_t lrow = doc->logical_lines.size();
		doc->logical_lines.push_back(l);
		cx()->line_index_map.insert(lrow, {});
		update_visual_line(lrow, false);
	}
	invalidate_nlines_cache();
}

void AbstractTextEditorApplication::write(char const *ptr, int len, bool by_keyboard)
{
	if (is_read_only()) return;

	char const *begin = ptr;
	char const *end = begin + (len < 0 ? strlen(ptr) : len);
	char const *left = begin;
	char const *right = begin;
	while (1) {
		int c = -1;
		if (right < end) {
			c = *right & 0xff;
		}
		if (c == '\n' || c == '\r' || c < 0) {
			utf8 src(left, right);
			while (1) {
				int d = src.next();
				if (d == 0) break;
				write(d, by_keyboard);
			}
			if (c < 0) break;
			right++;
			if (c == '\r') {
				c = isInsertMode() ? '\n' : '\r';
				if (right < end && *right == '\n') {
					c = '\n';
					right++;
				}
				write(c, by_keyboard);
			} else if (c == '\n') {
				write('\n', by_keyboard);
			}
			left = right;
		} else {
			right++;
		}
	}
}

void AbstractTextEditorApplication::write(std::string const &text)
{
	if (!text.empty()) {
		write(text.c_str(), (int)text.size(), false);
	}
}

void AbstractTextEditorApplication::write_(char const *ptr, bool by_keyboard)
{
	write(ptr, -1, by_keyboard);
}

void AbstractTextEditorApplication::write_(QString const &text, bool by_keyboard)
{
	if (is_read_only()) return;

	if (text.size() == 1) {
		ushort c = text.at(0).unicode();
		write(c, by_keyboard);
		return;
	}
	int len = text.size();
	if (len > 0) {
		ushort const *begin = text.utf16();
		ushort const *end = begin + len;
		ushort const *left = begin;
		ushort const *right = begin;
		while (1) {
			int c = -1;
			if (right < end) {
				c = *right;
			}
			if (c < 0x20) {
				if (left < right) {
					internalWrite(left, right);
				}
				if (c == -1) break;
				right++;
				if (c == '\n' || c == '\r') {
					if (c == '\r') {
						if (right < end && *right == '\n') {
							right++;
						}
					}
					writeNewLine();
				} else {
					write(c, by_keyboard);
				}
				left = right;
			} else {
				right++;
			}
		}
	}
}

void AbstractTextEditorApplication::write(QKeyEvent *e)
{
	setModifierKeys(e->modifiers());

	int c = e->key();
	if (c == Qt::Key_Backspace) {
		write(0x08, true);
	} else if (c == Qt::Key_Delete) {
		write(0x7f, true);
	} else if (c == Qt::Key_Up) {
		if (isControlModifierPressed()) {
			scrollUp();
		} else {
			write(EscapeCode::Up, true);
		}
	} else if (c == Qt::Key_Down) {
		if (isControlModifierPressed()) {
			scrollDown();
		} else {
			write(EscapeCode::Down, true);
		}
	} else if (c == Qt::Key_Left) {
		write(EscapeCode::Left, true);
	} else if (c == Qt::Key_Right) {
		write(EscapeCode::Right, true);
	} else if (c == Qt::Key_PageUp) {
		write(EscapeCode::PageUp, true);
	} else if (c == Qt::Key_PageDown) {
		write(EscapeCode::PageDown, true);
	} else if (c == Qt::Key_Home) {
		if (isControlModifierPressed()) {
			moveToTop();
		} else {
			write(EscapeCode::Home, true);
		}
	} else if (c == Qt::Key_End) {
		if (isControlModifierPressed()) {
			moveToBottom();
		} else {
			write(EscapeCode::End, true);
		}
	} else if (c == Qt::Key_Return || c == Qt::Key_Enter) {
		write('\n', true);
	} else if (c == Qt::Key_Escape) {
		write(0x1b, true);
	} else if (isControlModifierPressed()) {
		if (c < 0x80 && QChar(c).isLetter()) {
			c = QChar(c).toUpper().unicode();
			if (c >= 0x40 && c < 0x60) {
				write(c - 0x40, true);
			}
		}
	} else {
		QString text = e->text();
		write_(text, true);
	}
}
