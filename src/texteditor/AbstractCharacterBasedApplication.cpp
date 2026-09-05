#include "AbstractCharacterBasedApplication.h"
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

using WriteMode = AbstractCharacterBasedApplication::WriteMode;
using FormattedLine = AbstractCharacterBasedApplication::FormattedLine;

class EsccapeSequence {
private:
	int offset = 0;
	unsigned char data[100];
	int color_fg = -1;
	int color_bg = -1;
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

struct AbstractCharacterBasedApplication::Private {
	bool is_changed = false;
	bool is_quit_enabled = false;
	bool is_open_enabled = false;
	bool is_save_enabled = false;
	bool is_toggle_selection_anchor_enabled = true;
	bool is_read_only = false;
	bool is_terminal_mode = false;
	bool is_cursor_visible = true;
	State state = State::Normal;
	int screen_width = 80;
	int screen_height = 24;
	int content_width_px = -1;
	bool auto_layout = false;
	QString recently_used_path;
	bool show_line_number = true;
	int left_margin = AbstractCharacterBasedApplication::LEFT_MARGIN;
	QString dialog_title;
	QString dialog_value;
	std::vector<AbstractCharacterBasedApplication::Char16> screen;
	
	std::optional<std::vector<Character>> parsed_current_line_chars;
	
	bool dialog_mode = false;
	DialogHandler dialog_handler;
	bool is_painting_suppressed = false;
	int line_margin = 3;
	WriteMode write_mode = WriteMode::Insert;
	Qt::KeyboardModifiers keyboard_modifiers = Qt::KeyboardModifier::NoModifier;
	bool ctrl_modifier = false;
	bool shift_modifier = false;
	EsccapeSequence escape_sequence;

	bool cursor_moved_by_mouse = false;

	AbstractCharacterBasedApplication::WrappingMode wrapping_mode = AbstractCharacterBasedApplication::WrappingMode::NoWrap;
};

AbstractCharacterBasedApplication::AbstractCharacterBasedApplication()
	: m(new Private)
{
}

AbstractCharacterBasedApplication::~AbstractCharacterBasedApplication()
{
	delete m;
}

void AbstractCharacterBasedApplication::setModifierKeys(Qt::KeyboardModifiers const &keymod)
{
	m->keyboard_modifiers = keymod;
	m->ctrl_modifier = m->keyboard_modifiers & Qt::ControlModifier;
	m->shift_modifier = m->keyboard_modifiers & Qt::ShiftModifier;
}

void AbstractCharacterBasedApplication::clearShiftModifier()
{
	m->shift_modifier = false;
}

bool AbstractCharacterBasedApplication::isControlModifierPressed() const
{
	return m->ctrl_modifier;
}

bool AbstractCharacterBasedApplication::isShiftModifierPressed() const
{
	return m->shift_modifier;
}

void AbstractCharacterBasedApplication::setAutoLayout(bool f)
{
	m->auto_layout = f;
	layoutEditor();
}

void AbstractCharacterBasedApplication::showLineNumber(bool show, int left_margin)
{
	m->show_line_number = show;
	m->left_margin = left_margin;
}

void AbstractCharacterBasedApplication::setCursorVisible(bool show)
{
	m->is_cursor_visible = show;
}

bool AbstractCharacterBasedApplication::isCursorVisible()
{
	return m->is_cursor_visible;
}

bool AbstractCharacterBasedApplication::isChanged() const
{
	return m->is_changed;
}

void AbstractCharacterBasedApplication::setChanged(bool f)
{
	m->is_changed = f;
}

int AbstractCharacterBasedApplication::leftMargin_() const
{
	return m->left_margin;
}

void AbstractCharacterBasedApplication::setRecentlyUsedPath(QString const &path)
{
	m->recently_used_path = path;
}

QString AbstractCharacterBasedApplication::recentlyUsedPath()
{
	return m->recently_used_path;
}

void AbstractCharacterBasedApplication::makeBuffer()
{
	int w = screenWidth();
	int h = screenHeight();
	int size = w * h;
	m->screen.resize(size);
	std::fill(m->screen.begin(), m->screen.end(), Char16());
}

/**
 * @brief 物理行数を返す
 * @return
 */
row_index_t AbstractCharacterBasedApplication::nlines() const
{
	if (m->wrapping_mode == WrappingMode::NoWrap) {
		return logicalLines();
	} else {
		if (cx()->cache.nlines == std::nullopt) { // キャッシュが無効化されている場合は再計算する
			cx()->cache.nlines = cx()->line_index_map.total_visual_row_count();
		}
		return *cx()->cache.nlines;
	}
}

/**
 * @brief 物理行数キャッシュを無効化する
 */
void AbstractCharacterBasedApplication::invalidate_nlines_cache()
{
	cx()->cache.nlines = std::nullopt;
}

void AbstractCharacterBasedApplication::_update_logical_pos_cache() const
{
	TextEditorContext::Cache *cache = &cx()->cache; // mutable

	row_index_t vrow = current_visual_row();
	col_index_t vcol = current_visual_col();
	
	auto logical = cx()->line_index_map.visual_to_logical(vrow);
	cache->current_logical_row = logical.lrow;
	cache->current_logical_col = logical.lcol + vcol;
}

row_index_t AbstractCharacterBasedApplication::current_logical_row() const
{
	_update_logical_pos_cache();
	return cx()->cache.current_logical_row;
}

col_index_t AbstractCharacterBasedApplication::current_logical_col() const
{
	_update_logical_pos_cache();
	return cx()->cache.current_logical_col;
}

void AbstractCharacterBasedApplication::set_current_visual_row(row_index_t row)
{
	cx()->current_visual_row = row;
}

void AbstractCharacterBasedApplication::set_current_visual_col(col_index_t col)
{
	cx()->current_visual_col = col;
}

row_index_t AbstractCharacterBasedApplication::current_visual_row() const
{
	return cx()->current_visual_row;
}

int AbstractCharacterBasedApplication::current_visual_col() const
{
	return cx()->current_visual_col;
}

int AbstractCharacterBasedApplication::current_visual_pixel_x() const
{
	return cx()->current_visual_pixel_x;
}

int AbstractCharacterBasedApplication::scrollpos_row() const
{
	return cx()->scroll_pos_row;
}

int AbstractCharacterBasedApplication::scrollpos_col() const
{
	return cx()->scroll_pos_col;
}

void AbstractCharacterBasedApplication::setScrollPosRow(int row)
{
	cx()->scroll_pos_row = row;
}

void AbstractCharacterBasedApplication::setScrollPosCol(int col)
{
	cx()->scroll_pos_col = col;
}

int AbstractCharacterBasedApplication::cursor_col() const
{
	return current_visual_col() - scrollpos_col();
}

int AbstractCharacterBasedApplication::cursor_row() const
{
	return current_visual_row() - scrollpos_row();
}

Document::Line const *AbstractCharacterBasedApplication::currentLine() const
{
	row_index_t vrow = current_visual_row();
	return (vrow >= 0 && vrow < nlines()) ? line(vrow) : nullptr;
}

/**
 * @brief 桁位置を求める
 * @param cx
 * @param line
 * @return
 */
std::vector<Character> AbstractCharacterBasedApplication::_parseLine(TextEditorContext const *cx, Document::Line const *line, std::mutex *mutex) const
{
	if (!line) return {};
	
	std::vector<Character> ret;
	
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
	
	if (mutex) mutex->lock();
	calc_pos_x(&ret); // 内部でキャッシュを更新するのでmutexで保護する必要がある
	if (mutex) mutex->unlock();

	return ret;
}

std::vector<Character> AbstractCharacterBasedApplication::parseLine(Document::Line const *line, std::mutex *mutex) const
{
	return _parseLine(cx(), line, mutex);
}

std::vector<Character> AbstractCharacterBasedApplication::parseLine(row_index_t vrow) const
{
	if (vrow >= 0 && vrow < nlines()) {
		return parseLine(line(vrow));
	}
	return {};
}

void AbstractCharacterBasedApplication::clearParsedLine()
{
	m->parsed_current_line_chars = std::nullopt;
}

std::vector<Character> const &AbstractCharacterBasedApplication::parseCurrentLine(bool force)
{
	if (force || !m->parsed_current_line_chars) {
		m->parsed_current_line_chars = parseLine(current_visual_row());
	}
	return *m->parsed_current_line_chars;
}

void AbstractCharacterBasedApplication::setWrappingMode(WrappingMode mode)
{
	m->wrapping_mode = mode;
}

AbstractCharacterBasedApplication::WrappingMode AbstractCharacterBasedApplication::wrappingMode() const
{
	return m->wrapping_mode;
}

std::vector<Document::Line> AbstractCharacterBasedApplication::wrapLine(Document::Line line, std::mutex *mutex) const
{
	if (wrappingMode() == WrappingMode::NoWrap) return {};
	
	const int width_px = m->content_width_px;
	
	std::vector<std::vector<Character>> chrs_out;
	std::vector<Character> chrs_in = parseLine(&line, mutex);
	
	if (chrs_in.empty()) {
		chrs_out.push_back({});
	} else {
		int left_px = 0;
		int right_px = 0;

		auto Out = [&](size_t i, size_t n){
			std::vector<Character> chrs;
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
		int logical_col = 0;
		for (std::vector<Character> const &w : chrs_out) {
			std::vector<char> v;
			for (Character const &c : w) {
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
row_index_t AbstractCharacterBasedApplication::lrow_to_vrow(row_index_t lrow) const
{
	if (wrappingMode() == WrappingMode::NoWrap) {
		return lrow;
	}
	
	auto [vrow, vcol] = cx()->line_index_map.logical_to_visual(lrow, 0);
	return vrow;	
}

row_index_t AbstractCharacterBasedApplication::vrow_to_lrow(row_index_t vrow) const
{
	if (wrappingMode() == WrappingMode::NoWrap) {
		return vrow;
	}
	
	auto logical = cx()->line_index_map.visual_to_logical(vrow);
	return logical.lrow;
}

void AbstractCharacterBasedApplication::wrap_and_update_line_map(row_index_t lrow, Document::Line *ll, bool force, std::mutex *mutex)
{
	if (force || ll->sp->meta.visual_lines.empty()) { // 折り返し未処理の場合
		ll->sp->meta.visual_lines = wrapLine(*ll, mutex); // 折り返し処理
	}

	std::vector<uint32_t> col_list;
	for (Document::Line const &vl : ll->sp->meta.visual_lines) {
		col_list.push_back(vl.sp->meta.logical_col_len);
	}
	
	if (mutex) mutex->lock();
	cx()->line_index_map.update(lrow, col_list); // 論理行に対応する物理行数を更新
	if (mutex) mutex->unlock();
}

/**
 * @brief 物理行番号から論理行番号と論理列番号を求める。
 * @param vrow 物理行番号
 * @return 論理行番号と論理列番号を含むVisualRowInfo構造体
 */
VisualRowInfo AbstractCharacterBasedApplication::queryVisualRowInfo(row_index_t vrow)
{
	if (vrow < 0) return {};
	
	if (wrappingMode() == WrappingMode::NoWrap) {
		VisualRowInfo info;
		info.logical_row = vrow; // NoWrapの場合、論理行と物理行は同じ
		info.logical_col = 0;
		return info;
	}
	
	VisualRowInfo info;
	auto logical = cx()->line_index_map.visual_to_logical(vrow);
	info.logical_row = logical.lrow;
	info.logical_col = logical.lcol;
	return info;
}

/**
 * @brief 物理行番号に対応する論理行番号と論理列番号を更新する
 * @param vrow 物理行番号
 */
void AbstractCharacterBasedApplication::updateVisualRow(row_index_t vrow)
{
	queryVisualRowInfo(vrow);
}

/**
 * @brief 論理行に対応する物理行情報を更新する
 * @param lrow 論理行インデックス
 * @param ll 論理行情報
 * @param mutex 排他制御用のmutex（nullptrの場合は排他制御なし）
 */
void AbstractCharacterBasedApplication::_updateVisualLineByLogicalLine(col_index_t lrow, Document::Line const &ll, std::mutex *mutex)
{
	if (mutex) {
		std::lock_guard lock(*mutex);
		_updateVisualLineByLogicalLine(lrow, ll, nullptr);
		return;
	}
	
	std::vector<Document::Line> const &src = ll.sp->meta.visual_lines;
	
	TextEditorContext *cx = this->cx();
	
	auto [lower_pos, lower_vcol] = cx->line_index_map.logical_to_visual(lrow, 0); // 論理行に対応する物理行の開始位置
	auto [upper_pos, upper_vcol] = cx->line_index_map.logical_to_visual(lrow + 1, 0); // 論理行に対応する物理行の終了位置
	(void)lower_vcol;
	(void)upper_vcol;
	
	const size_t dstlen = upper_pos - lower_pos; // 論理行に対応する物理行の数
	const size_t srclen = src.size(); // 折り返し処理後の物理行の数

	const size_t nvlines = cx->cache.visual_lines.size(); // 現在の物理行数
	
	if (dstlen > srclen) { // 書き込み先の方が長い場合、余分な物理行を削除する
		size_t erase_begin = std::min(lower_pos + srclen, nvlines);
		size_t erase_end = std::min((size_t)upper_pos, nvlines);
		if (erase_begin < erase_end) {
			cx->cache.visual_lines.erase(cx->cache.visual_lines.begin() + erase_begin, cx->cache.visual_lines.begin() + erase_end);
		}
	} else {
		size_t insert_begin = std::min(lower_pos + dstlen, nvlines);
		size_t insert_end = lower_pos + srclen;
		if (insert_begin > nvlines) { // 書き込み先の方が短い場合、足りない分を追加する
			cx->cache.visual_lines.resize(insert_begin);
		}
		size_t n = insert_end - insert_begin; // 追加する物理行数
		if (n > 0) {
			cx->cache.visual_lines.insert(cx->cache.visual_lines.begin() + insert_begin, n, {});
		}
	}

	if (!src.empty()) {
		// 物理行情報を更新する
		auto dst = cx->cache.visual_lines.begin() + lower_pos;
		std::copy(src.begin(), src.end(), dst);
	}
}

bool AbstractCharacterBasedApplication::_update_visual_line(row_index_t lrow, std::optional<std::vector<char>> text, bool force, std::mutex *mutex)
{
	bool vline_count_changed = false;
	if (m->wrapping_mode != WrappingMode::NoWrap) {
		Document *doc = &cx()->engine->document;
		std::vector<Document::Line> *llines = &doc->logical_lines;
		if (lrow >= 0 && lrow < llines->size()) {
			Document::Line *ll = &(*llines)[lrow];

			const size_t nvlines = ll->sp->meta.visual_lines.size(); // 折り返し前の物理行数
			
			if (text) { // テキストが指定されている場合、論理行のテキストを更新する
				ll->set_text(*text);
				ll->clear_visual_lines();
				ll->sp->meta.detail.reset();
			}
			
			wrap_and_update_line_map(lrow, ll, force, mutex); // 折り返し処理と物理行情報の更新
			
			vline_count_changed = (nvlines != ll->sp->meta.visual_lines.size()); // 折り返し後の物理行数が変化したかどうか
		}
	}
	return vline_count_changed;
}

void AbstractCharacterBasedApplication::update_visual_line(row_index_t lrow, bool force)
{
	bool vline_count_changed = _update_visual_line(lrow, std::nullopt, force, nullptr);

	std::vector<Document::Line> *llines = &document()->logical_lines;
	if (lrow >= 0 && lrow < llines->size()) {
		Document::Line *ll = &(*llines)[lrow];
		_updateVisualLineByLogicalLine(lrow, *ll, nullptr);
	}
	
	if (vline_count_changed) {
		invalidate_nlines_cache();
	}
}

void AbstractCharacterBasedApplication::_updateVisualLinesAll(bool force)
{
	TextEditorContext *cx = this->cx();
	
	if (m->wrapping_mode == WrappingMode::NoWrap) {
		cx->cache.visual_lines = {};
		return;
	}
	
	
	if (force) {
		cx->line_index_map.clear();
	}
	
	{
		std::vector<Document::Line> *llines = &cx->engine->document.logical_lines;
		
		if (1) {
			for (row_index_t lrow = 0; lrow < (row_index_t)llines->size(); lrow++) {
				update_visual_line(lrow, force);
			}
		} else {
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
						_update_visual_line(lrow, std::nullopt, force, &mutex);
					}
				});
			}
			for (int i = 0; i < nthreads; i++) {
				thread[i].join();
			}
			for (row_index_t lrow = 0; lrow < (row_index_t)llines->size(); lrow++) {
				std::vector<Document::Line> *llines = &document()->logical_lines;
				Document::Line *ll = &(*llines)[lrow];
				_updateVisualLineByLogicalLine(lrow, *ll, nullptr);
			}
		}
	}
	
	//
	
	std::vector<Document::Line> const &llines = cx->engine->document.logical_lines;
	
	row_index_t vrow = 0;	
	for (row_index_t lrow = 0; lrow < (row_index_t)llines.size(); lrow++) {
		Document::Line const &ll = llines[lrow];
		_updateVisualLineByLogicalLine(lrow, ll, nullptr);
		vrow++;
	}
	updateVisualRow(vrow);
	
	updateScrollBarRange();
}

void AbstractCharacterBasedApplication::invalidateVisualRowInfo(row_index_t vrow)
{
	TextEditorContext *cx = this->cx();
	if (vrow < 0) {
		cx->cache.visual_lines.clear();
		cx->line_index_map.clear();
	} else {
		cx->cache.visual_lines.resize(vrow);
	}
}

bool AbstractCharacterBasedApplication::commit_line(row_index_t lrow, std::vector<Character> const &vec)
{
	std::vector<Document::Line> *llines = documentLinesForWrite();
	if (!llines) return false;
	
	std::vector<char> ba;
	if (!vec.empty()){
		std::vector<char32_t> v;
		v.reserve(vec.size());
		for (Character const &c : vec) {
			v.push_back(c.unicode);
		}
		utf32 u32(&v[0], v.size());
		u32.to_utf8([&](char c, int pos){
			(void)pos;
			ba.push_back(c);
			return true;
		});
	}

	if (lrow == llines->size()) {
		Document::Line newline;
		newline.sp->meta.type = Document::LineType::Normal;
		llines->push_back(newline);
	}
	
	clearParsedLine();
	
	bool vline_count_changed  = _update_visual_line(lrow, ba, false, nullptr);
	
	if (vline_count_changed) {
		invalidate_nlines_cache();
	}
	
	return vline_count_changed;
}

void AbstractCharacterBasedApplication::closeDialog(bool result)
{
	if (isDialogMode()) {
		deselect();
		QString line;
		if (!dialog_cx->engine->document.logical_lines.empty()) {
			Document::Line const &l = dialog_cx->engine->document.logical_lines.front();
			line = QString::fromUtf8(l.text().data(), (int)l.text().size());
		}
		setDialogMode(false);
		if (m->dialog_handler) {
			m->dialog_handler(result, line);
		}
		return;
	}
}

void AbstractCharacterBasedApplication::layoutEditor()
{
	makeBuffer();
	editor_cx->viewport_org_x = leftMargin_();
	editor_cx->viewport_org_y = 0;
	editor_cx->viewport_width = screenWidth() - cx()->viewport_org_x;
	editor_cx->viewport_height = screenHeight();
}

void AbstractCharacterBasedApplication::initEditor()
{
	editor_cx = std::make_shared<TextEditorContext>();
	layoutEditor();
}

bool AbstractCharacterBasedApplication::isLineNumberVisible() const
{
	return m->show_line_number;
}

int AbstractCharacterBasedApplication::charWidth(uint32_t c)
{
	return UnicodeWidth::width(UnicodeWidth::type(c));
}

std::vector<FormattedLine> AbstractCharacterBasedApplication::formatLine_(Document::Line const &line, int tab_indent_size, int anchor_a, int anchor_b) const
{
	std::vector<FormattedLine> ret;
	
	std::vector<char16_t> c16vec;
	size_t len = line.text().size();
	c16vec.reserve(len + 100);
	
	int col = 0;
	int col_start = col;
	
	// bool flag_a = false;
	// bool flag_b = false;
	
	auto Flush = [&](size_t offset, size_t *next_offset){
		if (!c16vec.empty()) {
			int atts = 0;
			if (anchor_a >= 0 || anchor_b >= 0) {
				if ((anchor_a < 0 || col_start >= anchor_a) && (anchor_b == -1 || col_start < anchor_b)) {
					atts |= FormattedLine::Selected;
				}
			}
			char16_t const *left = &c16vec[0];
			char16_t const *right = left + c16vec.size();
			while (left < right && (right[-1] == '\r' || right[-1] == '\n')) right--;
			if (left < right) {
				ret.push_back(FormattedLine(QString::fromUtf16(left, int(right - left)), atts));
			}
			c16vec.clear();
		} else {
			*next_offset = (size_t)-1;
		}
		col_start = col;
	};
	
	// size_t offset = 0;
	// size_t next_offset = (size_t)-1;
	if (len > 0) {
		utf8 u8(line.text().data(), len);
		u8.to_utf32([&](uint32_t c){
			// if (line.byte_offset + u8.offset() == next_offset) {
			// 	Flush(line.byte_offset + offset, &next_offset);
			// 	offset += u8.offset();
			// }
			if (c == '\t') {
				do {
					c16vec.push_back(' ');
					col++;
				} while (col % tab_indent_size != 0);
			} else if (c < ' ') {
				// nop
			} else {
				int cw = charWidth(c);
				if (c < 0xffff) {
					c16vec.push_back((ushort)c);
				} else {
					unsigned int a = c >> 16;
					if (a > 0 && a <= 0x20) {
						a--;
						unsigned int b = c & 0x03ff;
						a = (a << 6) | ((c >> 10) & 0x003f);
						a |= 0xd800;
						b |= 0xdc00;
						c16vec.push_back((ushort)a);
						c16vec.push_back((ushort)b);
					}
				}
				col += cw;
			}
			// if ((anchor_a >= 0 || anchor_b >= 0) && anchor_a != anchor_b) {
			// 	if (!flag_a && col >= anchor_a) {
			// 		Flush(line.byte_offset + offset, &next_offset);
			// 		flag_a = true;
			// 	}
			// 	if (!flag_b && col >= anchor_b) {
			// 		Flush(line.byte_offset + offset, &next_offset);
			// 		flag_b = true;
			// 	}
			// }
			return true;
		});
	}
	// Flush(line.byte_offset + offset, &next_offset);
	{
		
		int atts = 0;

		ret.push_back(FormattedLine(QString::fromUtf16((ushort const *)c16vec.data(), c16vec.size()), atts));
	}
	return ret;
}

std::vector<Document::Line> *AbstractCharacterBasedApplication::_lines()
{
	assert(0); // TODO:
	
	if (m->wrapping_mode == WrappingMode::NoWrap) {
		return &cx()->engine->document.logical_lines;
	} else {
		return &cx()->cache.visual_lines;
	}
}

Document::Line *AbstractCharacterBasedApplication::line(row_index_t vrow)
{
	std::vector<Document::Line> *lines = nullptr;
	if (m->wrapping_mode == WrappingMode::NoWrap) {
		lines = &document()->logical_lines;
	} else {
		size_t end = std::min(vrow + 1, nlines());
		for (size_t r = cx()->cache.visual_lines.size(); r < end; r++) {
			cx()->cache.visual_lines.push_back({});
			update_visual_line(r, true);
		}
		lines = &cx()->cache.visual_lines;
	}
	if (vrow >= 0 && vrow < (row_index_t)lines->size()) {
		return &(*lines)[vrow];
	}
	return nullptr;
}

int AbstractCharacterBasedApplication::screenWidth() const
{
	return m->screen_width;
}

int AbstractCharacterBasedApplication::screenHeight() const
{
	return m->screen_height;
}

void AbstractCharacterBasedApplication::setScreenSize(int w, int h, bool update_layout)
{
	m->screen_width = w;
	m->screen_height = h;
	if (update_layout) {
		layoutEditor();
	}
}

void AbstractCharacterBasedApplication::setContentWidth(int w)
{
	m->content_width_px = w;
}

bool AbstractCharacterBasedApplication::isPaintingSuppressed() const
{
	return m->is_painting_suppressed;
}

void AbstractCharacterBasedApplication::setPaintingSuppressed(bool f)
{
	m->is_painting_suppressed = f;
}

std::vector<Document::Line> *AbstractCharacterBasedApplication::documentLinesForWrite(bool check_readonly)
{
	if (check_readonly && isReadOnly()) return nullptr;
	return &document()->logical_lines;
}

void AbstractCharacterBasedApplication::setDocument(std::vector<Document::Line> const *source)
{
	std::vector<Document::Line> *lines = documentLinesForWrite(false);
	if (!lines) return;
	
	if (source) {
		*lines = *source;
	} else {
		lines->clear();
	}
}

void AbstractCharacterBasedApplication::insertLine(row_index_t lrow)
{
	std::vector<Document::Line> *llines = documentLinesForWrite();
	if (!llines) return;

	llines->insert(llines->begin() + lrow, Document::Line::NormalEmptyLine());
	cx()->line_index_map.insert(lrow, {});

	invalidate_nlines_cache();
}

std::vector<Character> AbstractCharacterBasedApplication::parseLogicalLine(TextEditorContext const *cx, row_index_t lrow) const
{
	std::vector<Document::Line> const &lines = cx->engine->document.logical_lines;
	if (lrow >= 0 && lrow < lines.size()) {
		Document::Line const *line = &lines[lrow];
		return parseLine(line);
	}
	return {};
}

bool AbstractCharacterBasedApplication::isCurrentLineWritable() const
{
	if (isReadOnly()) return false;

	row_index_t vrow = current_visual_row();
	if (vrow >= 0 && vrow < nlines()) {
		if (line(vrow)->sp->meta.type != Document::LineType::Invalid) {
			return true;
		}
	}
	return false;
}

int AbstractCharacterBasedApplication::editorViewportWidth() const
{
	return cx()->viewport_width;
}

int AbstractCharacterBasedApplication::editorViewportHeight() const
{
	return cx()->viewport_height;
}

int AbstractCharacterBasedApplication::print(int x, int y, QString const &text, const AbstractCharacterBasedApplication::Option &opt)
{
	CharAttr attr = opt.char_attr;
	if (opt.char_flag.selected) {
		attr.index = CharAttr::Invert;
	}
	if (opt.char_flag.current_line) {
		attr.index = CharAttr::Hilite;
	}

	const int screen_w = screenWidth();
	const int screen_h = screenHeight();
	int x_start = 0;
	int y_start = 0;
	int x_limit = screen_w;
	int y_limit = screen_h;
	if (!opt.clip.isNull()) {
		x_start = opt.clip.x();
		y_start = opt.clip.y();
		x_limit = x_start + opt.clip.width();
		y_limit = y_start + opt.clip.height();
	}
	if (y >= y_start && y < y_limit) {
		bool changed = false;
		int x2 = x;
		int y2 = y;
		if (text.isEmpty()) {
			changed = true; // set changed flag if text is empty
		} else {
			for (int i = 0; i < text.size(); i++) {
				ushort c = text.utf16()[i];
				if (c < ' ' || c == 0x7f) continue;
				int cw = charWidth(c);
				if (x2 + cw > x_limit) {
					break;
				}
				for (int j = 0; j < cw; j++) {
					if (x2 >= x_start && x2 < screen_w) {
						int o = y2 * screen_w + x2;
						CharAttr a;
						if (j == 0) {
							a = attr;
						} else {
							c = -1;
						}
						if (c != m->screen[o].c || a != m->screen[o].a) {
							m->screen[o].c = c;
							m->screen[o].a = a;
							changed = true;
						}
					}
					x2++;
				}
			}
		}

		x = x2;
	}
	return x;
}

void AbstractCharacterBasedApplication::initEngine(std::shared_ptr<TextEditorContext> const &cx)
{
	cx->engine = std::make_shared<TextEditorEngine>();
}

TextEditorContext *AbstractCharacterBasedApplication::cx()
{
	if (dialog_cx) {
		if (!dialog_cx->engine) {
			initEngine(dialog_cx);
		}
		return dialog_cx.get();
	}
	if (!editor_cx->engine) {
		initEngine(editor_cx);
	}
	return editor_cx.get();
}

const TextEditorContext *AbstractCharacterBasedApplication::cx() const
{
	return const_cast<AbstractCharacterBasedApplication *>(this)->cx();
}

TextEditorEngine_sp AbstractCharacterBasedApplication::engine() const
{
	Q_ASSERT(cx()->engine);
	return cx()->engine;
}

void AbstractCharacterBasedApplication::setTextEditorEngine(TextEditorEngine_sp const &e)
{
	cx()->engine = e;
}

void AbstractCharacterBasedApplication::clear()
{
	setDocument(nullptr);
}

void AbstractCharacterBasedApplication::writeNewLine()
{
	if (isReadOnly()) return;
	if (isSingleLineMode()) return;

	row_index_t vrow = current_visual_row();
	row_index_t lrow = current_logical_row();
	col_index_t lcol = current_logical_col();
	
	invalidateVisualRowInfo(vrow);
	
	std::vector<Character> curr_line;
	std::vector<Character> next_line;
	
	curr_line = parseLogicalLine(cx(), lrow);
	
	// 行を分割
	next_line.insert(next_line.end(), curr_line.begin() + lcol, curr_line.end());
	curr_line.resize(lcol);
	curr_line.emplace_back('\n');
	
	// 現在の行を確定
	commit_line(lrow, curr_line);

	// 次の行を挿入
	lrow++;
	insertLine(lrow);
	commit_line(lrow, next_line);
	
	vrow++;
	set_current_visual_row(vrow);

	setCursorCol(0);
	clearParsedLine();
	updateVisibility(true, true, true);
}

void AbstractCharacterBasedApplication::openFile(QString const &path)
{
	document()->logical_lines.clear();
	QFile file(path);
	if (file.open(QFile::ReadOnly)) {
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
		setRecentlyUsedPath(path);
	}

	if (document()->logical_lines.empty()) {
		Document::Line line;
		line.sp->meta.type = Document::LineType::Normal;
		document()->logical_lines.push_back(line);
	}

	updateVisualLinesAll();
	
	scrollToTop();
}

void AbstractCharacterBasedApplication::saveFile(QString const &path)
{
	QFile file(path);
	if (file.open(QFile::WriteOnly)) {
		for (Document::Line const &line : document()->logical_lines) {
			file.write(line.text().data(), line.text().size());
		}
	}
}

void AbstractCharacterBasedApplication::pressEnter()
{
	deleteIfSelected();

	if (isDialogMode()) {
		closeDialog(true);
	} else {
		writeNewLine();
	}
}

void AbstractCharacterBasedApplication::pressEscape()
{
	if (isTerminalMode()) {
		m->escape_sequence.write(0x1b);
		return;
	}

	if (isDialogMode()) {
		closeDialog(false);
		return;
	}

	deselect();
	updateVisibility(false, false, false);
}

AbstractCharacterBasedApplication::State AbstractCharacterBasedApplication::state() const
{
	return m->state;
}

Document *AbstractCharacterBasedApplication::document()
{
	return &engine()->document;
}

Document const *AbstractCharacterBasedApplication::document() const
{
	return &engine()->document;
}

int AbstractCharacterBasedApplication::logicalLines() const
{
	return document()->logical_lines.size();
}

bool AbstractCharacterBasedApplication::isSingleLineMode() const
{
	return cx()->single_line;
}

void AbstractCharacterBasedApplication::setLineMargin(int n)
{
	m->line_margin = n;
}

void AbstractCharacterBasedApplication::ensureCurrentLineVisible()
{
	int margin = (cx()->viewport_height >= 6 && !isSingleLineMode()) ? m->line_margin : 0;
	int pos = scrollpos_row();
	int top = current_visual_row() - margin;
	int bottom = current_visual_row() + 1 - editorViewportHeight() + margin;
	if (pos > top)    pos = top;
	if (pos < bottom) pos = bottom;
	if (pos < 0) pos = 0;
	if (scrollpos_row() != pos) {
		setScrollPosRow(pos);
	}
}

bool AbstractCharacterBasedApplication::isWidthFixed() const
{
	return (wrappingMode() == WrappingMode::CharWrap);
}

int AbstractCharacterBasedApplication::decideColumnScrollPos() const
{//@
	if (isWidthFixed()) return 0;

	int x = current_visual_col();
	int w = editorViewportWidth() - RIGHT_MARGIN;
	if (w < 0) w = 0;
	return x > w ? (current_visual_col() - w) : 0;
}

int AbstractCharacterBasedApplication::calcVisualWidth(const Document::Line &line) const
{
	std::vector<FormattedLine> lines = formatLine_(line, cx()->tab_indent_size);
	int x = 0;
	for (FormattedLine const &line : lines) {
		if (line.text.isEmpty()) continue;
		ushort const *ptr = line.text.utf16();
		ushort const *end = ptr + line.text.size();
		while (1) {
			int c = -1;
			if (ptr < end) {
				c = *ptr;
				ptr++;
			}
			if (c == -1 || c == '\r' || c == '\n') {
				break;
			}
			x++;
		}
	}
	return x;
}



void AbstractCharacterBasedApplication::clearRect(int x, int y, int w, int h)
{
	int scr_w = screenWidth();
	int scr_h = screenHeight();
	int y0 = y;
	int y1 = y + h;
	if (y0 < 0) y0 = 0;
	if (y1 > scr_h) y1 = scr_h;
	int x0 = x;
	int x1 = x + w;
	if (x0 < 0) x0 = 0;
	if (x1 > scr_w) x1 = scr_w;
	for (int y = y0; y < y1; y++) {
		for (int x = x0; x < x1; x++) {
			int o = y * scr_w + x;
			m->screen[o] = Char16();
		}
	}
}

void AbstractCharacterBasedApplication::savePos()
{
	TextEditorContext *p = editor_cx.get();
	if (p) {
		p->saved_row = current_visual_row();
		p->saved_col = current_visual_col();
		p->saved_col_hint = p->current_visual_col_hint;
	}
}

void AbstractCharacterBasedApplication::restorePos()
{
	TextEditorContext *p = editor_cx.get();
	if (p) {
		set_current_visual_row(p->saved_row);
		set_current_visual_col(p->saved_col);
		p->current_visual_col_hint = p->saved_col_hint;
	}
}

void AbstractCharacterBasedApplication::deselect()
{
	selection_end = {};
	selection_start = {};
}

bool AbstractCharacterBasedApplication::hasSelection() const
{
	return !selection_end;
}

void AbstractCharacterBasedApplication::updateSelectionAnchor1(bool auto_scroll)
{
	if (isShiftModifierPressed()) {
		if (!selection_end) {
			setSelectionAnchor(true, true, auto_scroll);
			selection_start = selection_end;
		}
	} else if (selection_end) {
		// 選択中でShiftが押されていなければ選択解除
		setSelectionAnchor(false, false, auto_scroll);
	}
}

void AbstractCharacterBasedApplication::updateSelectionAnchor2(bool auto_scroll)
{
	if (selection_end) {
		// 選択中なら、現在位置で更新
		setSelectionAnchor(true, true, auto_scroll);
	}
}

void AbstractCharacterBasedApplication::setCursorRow(row_index_t vrow, bool auto_scroll, bool by_mouse)
{
	if (current_visual_row() == vrow) return;

	updateSelectionAnchor1(false);

	set_current_visual_row(vrow);

	updateSelectionAnchor2(auto_scroll);

	m->cursor_moved_by_mouse = by_mouse;
}

void AbstractCharacterBasedApplication::setCursorCol_(col_index_t vcol, bool auto_scroll, bool by_mouse)
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

int AbstractCharacterBasedApplication::nextTabStop(const TextEditorContext *cx, int x)
{
	x += cx->tab_indent_size;
	x -= x % cx->tab_indent_size;
	return x;
}

void AbstractCharacterBasedApplication::editSelected(EditOperation op, std::vector<Character> *cutbuffer)
{
	if (isReadOnly() && op == EditOperation::Cut) {
		op = EditOperation::Copy;
	}

	SelectionAnchor a = selection_end;
	SelectionAnchor b = selection_start;
	if (!a) return;
	if (!b) return;
	if (a == b) return;

	auto UpdateVisibility = [&](){
		updateVisibility(false, false, false);
	};

	if (cutbuffer) {
		cutbuffer->clear();
	}
	std::list<std::vector<Character>> cutlist;

	if (a.vrow > b.vrow) {
		std::swap(a, b);
	} else if (a.vrow == b.vrow) {
		if (a.vcol > b.vcol) {
			std::swap(a, b);
		}
	}

	int curr_row = current_visual_row();
	int curr_col = current_visual_col();

	set_current_visual_row(b.vrow);
	set_current_visual_col(b.vcol);

	if (a.vrow == b.vrow) {
		std::vector<Character> chars = parseCurrentLine(true);
		auto begin = chars.begin() + calcColumnToIndex(a.vcol);
		auto end = chars.begin() + calcColumnToIndex(b.vcol);
		if (cutbuffer) {
			std::vector<Character> cut;
			cut.insert(cut.end(), begin, end);
			cutlist.push_back(std::move(cut));
		}
		if (op == EditOperation::Cut) {
			chars.erase(begin, end);
			commit_line(current_logical_row(), chars);
			UpdateVisibility();
		}
	} else {
		std::vector<Character> chars = parseCurrentLine(true);
		{
			auto begin = chars.begin();
			auto end = chars.begin() + calcColumnToIndex(b.vcol);
			if (cutbuffer) {
				std::vector<Character> cut;
				cut.insert(cut.end(), begin, end);
				cutlist.push_back(std::move(cut));
			}
			if (op == EditOperation::Cut) {
				chars.erase(begin, end);
				commit_line(current_logical_row(), chars);
				UpdateVisibility();
			}
		}
		int n = b.vrow - a.vrow;
		for (int i = 0; i < n; i++) {
			if (cutbuffer && i > 0) {
				set_current_visual_row(b.vrow - i);
				set_current_visual_col(0);
				std::vector<Character> const &chars = parseCurrentLine(true);
				cutlist.push_back(chars);
			}
			if (op == EditOperation::Cut) {
				_lines()->erase(_lines()->begin() + b.vrow - i);
			}
		}

		set_current_visual_row(a.vrow);
		set_current_visual_col(a.vcol);
		int index = calcColumnToIndex(a.vcol);
		std::vector<Character> chars2 = parseCurrentLine(true);
		if (cutbuffer) {
			std::vector<Character> cut;
			cut.insert(cut.end(), chars2.begin() + index, chars2.end());
			cutlist.push_back(std::move(cut));
		}

		if (op == EditOperation::Cut) {
			chars2.resize(index);
			chars2.insert(chars2.end(), chars.begin(), chars.end());
			commit_line(current_logical_row(), chars2);
			UpdateVisibility();
		}
	}

	if (cutbuffer) {
		size_t size = 0;
		for (std::vector<Character> const &v : cutlist) {
			size += v.size();
		}
		cutbuffer->reserve(size);
		for (auto it = cutlist.rbegin(); it != cutlist.rend(); it++) {
			std::vector<Character> const &v = *it;
			cutbuffer->insert(cutbuffer->end(), v.begin(), v.end());
		}
	}

	if (op == EditOperation::Cut) {
		deselect();
		setCursorPos(a.vrow, a.vcol);
	} else {
		set_current_visual_row(curr_row);
		set_current_visual_col(curr_col);
	}

	clearParsedLine();
	UpdateVisibility();
}

void AbstractCharacterBasedApplication::edit_(EditOperation op)
{
	std::vector<Character> cutbuf;
	editSelected(op, &cutbuf);
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

bool AbstractCharacterBasedApplication::deleteIfSelected()
{
	if (selection_end && selection_start) {
		if (selection_end != selection_start) {
			editSelected(EditOperation::Cut, nullptr);
			return true;
		}
	}
	return false;
}

void AbstractCharacterBasedApplication::doDelete()
{
	if (isReadOnly()) return;
	if (isTerminalMode()) return;

	if (deleteIfSelected()) {
		return;
	}
	
	Document *doc = document();
	
	col_index_t lrow = current_logical_row();
	col_index_t lcol = current_logical_col();
	std::vector<Character> vec = parseLogicalLine(cx(), lrow);
	bool delete_nl = false; // 削除した文字が改行コードであるかどうか
	char32_t c = -1;
	if (lcol >= 0 && lcol < (int)vec.size()) {
		c = (vec)[lcol].unicode;
	}
	if (c == '\n' || c == '\r' || c == -1) {
		if (isSingleLineMode()) return;
		if (c != -1) {
			vec.erase(vec.begin() + lcol);
			if (c == '\r' && lcol < (int)vec.size() && (vec)[lcol].unicode == '\n') {
				vec.erase(vec.begin() + lcol);
			}
		}
		delete_nl = true;
		if (lcol == (int)vec.size()) {
			row_index_t next_lrow = lrow + 1;
			std::vector<Character> next = parseLogicalLine(cx(), next_lrow);
			vec.insert(vec.end(), next.begin(), next.end());
			if (next_lrow < logicalLines()) {
				doc->logical_lines.erase(doc->logical_lines.begin() + next_lrow);
				cx()->line_index_map.erase(next_lrow);
				invalidate_nlines_cache();
			}
		}
	} else {
		vec.erase(vec.begin() + lcol);
	}

	row_index_t vrow = lrow_to_vrow(lrow);
	col_index_t vcol = current_visual_col();
	
	if (commit_line(lrow, vec)) {
		// 折り返し後の物理行数が変化した場合は、これ以降の物理行情報を無効化する
		if (delete_nl) {
			invalidateVisualRowInfo(vrow);
		} else {
			invalidateVisualRowInfo(vrow + 1);
		}
	}
	
	vcol = lcol; // 論理行から物理行を再計算
	std::vector<Document::Line> *llines = &doc->logical_lines;
	Document::Line const &line = (*llines)[lrow];
	for (size_t i = 0; i < line.sp->meta.visual_lines.size(); i++) {
		std::vector<Character> chars = parseLine(&line.sp->meta.visual_lines[i]);
		if (vcol <= chars.size()) break;
		vcol -= chars.size();
		vrow++;
	}
	if (nlines() > 0 && vrow >= nlines()) {
		vrow = nlines() - 1;
	}
	setCursorPos(vrow, vcol);

	updateVisibility(true, true, true);
}

void AbstractCharacterBasedApplication::doBackspace()
{
	if (isReadOnly()) return;
	if (isTerminalMode()) return;

	if (deleteIfSelected()) {
		return ;
	}

	if (current_visual_row() > 0 || current_visual_col() > 0) {
		setPaintingSuppressed(true);
		moveCursorLeft();
		doDelete();
		setPaintingSuppressed(false);
		updateVisibility(true, true, true);
	}
}

bool AbstractCharacterBasedApplication::isDialogMode()
{
	return m->dialog_mode;
}

void AbstractCharacterBasedApplication::setDialogOption(QString const &title, QString const &value, DialogHandler const &handler)
{
	m->dialog_title = title;
	m->dialog_value = value;
	m->dialog_handler = handler;
}

void AbstractCharacterBasedApplication::setDialogMode(bool f)
{
	if (f) {
		if (!dialog_cx) {
			int y = screenHeight() - 2;
			dialog_cx = std::make_shared<TextEditorContext>();
			dialog_cx->engine = std::make_shared<TextEditorEngine>();
			dialog_cx->single_line = true;
			dialog_cx->viewport_org_x = 0;
			dialog_cx->viewport_org_y = y + 1;
			dialog_cx->viewport_width = screenWidth();
			dialog_cx->viewport_height = 1;
		}
		dialog_cx->engine->document.logical_lines.push_back(Document::Line(m->dialog_value.toUtf8()));
		editor_cx->viewport_height = screenHeight() /* - m->header_line*/ - 2;
		m->dialog_mode = true;
		clearParsedLine();
		moveCursorEnd();
	} else {
		dialog_cx.reset();
		m->dialog_mode = false;
		layoutEditor();
		clearParsedLine();
		updateVisibility(true, true, true);
	}
}

void AbstractCharacterBasedApplication::execDialog(QString const &dialog_title, QString const &dialog_value, DialogHandler const &handler)
{
	setDialogOption(dialog_title, dialog_value, handler);
	setDialogMode(true);
}

int AbstractCharacterBasedApplication::calcColumnToIndex(int column)
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

int AbstractCharacterBasedApplication::scrollBottomLimit() const
{
	return logicalLines() - editorViewportHeight() / 2;
}

int AbstractCharacterBasedApplication::scrollBottomLimit2() const
{
	return logicalLines() - editorViewportHeight();
}

void AbstractCharacterBasedApplication::moveCursorOut()
{
	setCursorRow(-1);
}

void AbstractCharacterBasedApplication::moveCursorHome(bool consider_indent)
{
	col_index_t vcol = 0;

	if (consider_indent) { // 行頭の空白を飛ばす
		row_index_t vrow = current_visual_row();
		VisualRowInfo info = queryVisualRowInfo(vrow);
		if (vrow == lrow_to_vrow(current_logical_row())) { // 論理行の先頭なら
			std::vector<Character> const &vline = parseCurrentLine(false);
			const col_index_t ncols = vline.size();
			col_index_t indent_vcol = 0;
			while (indent_vcol < ncols) {
				char32_t c = vline[indent_vcol].unicode;
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

	setCursorCol(vcol);
	clearParsedLine();
	updateVisibility(true, true, true);
}

void AbstractCharacterBasedApplication::moveCursorEnd()
{
	std::vector<Character> const &vline = parseCurrentLine(false);
	col_index_t col = vline.size();
	
	while (col > 0) { // 行末の改行コードを飛ばす
		char32_t c = vline[col - 1].unicode;
		if (c == '\r' || c == '\n') {
			col--;
		} else {
			break;
		}
	}
	
	setCursorCol(col);
	clearParsedLine();
	updateVisibility(true, true, true);
}

void AbstractCharacterBasedApplication::scrollUp()
{
	if (scrollpos_row() > 0) {
		setScrollPosRow(scrollpos_row() - 1);
		clearParsedLine();
		updateVisibility(false, false, true);
	}
}

void AbstractCharacterBasedApplication::scrollDown()
{
	int limit = scrollBottomLimit();
	if (scrollpos_row() < limit) {
		setScrollPosRow(scrollpos_row() + 1);
		clearParsedLine();
		updateVisibility(false, false, true);
	}
}

void AbstractCharacterBasedApplication::moveCursorUp()
{
	if (isSingleLineMode()) {
		// nop
	} else if (current_visual_row() > 0) {
		setCursorRow(current_visual_row() - 1); // カーソルを1行上へ
		clearParsedLine();
		updateVisibility(true, false, true);
	}
}

void AbstractCharacterBasedApplication::moveCursorDown()
{
	if (isSingleLineMode()) {
		// nop
	} else if (current_visual_row() + 1 < (int)nlines()) {
		setCursorRow(current_visual_row() + 1); // カーソルを1行下へ
		clearParsedLine();
		updateVisibility(true, false, true);
	}
}

void AbstractCharacterBasedApplication::scrollToTop()
{
	if (isSingleLineMode()) return;

	setCursorRow(0);
	setCursorCol(0);
	setScrollPosRow(0);
	clearParsedLine();
	updateVisibility(true, false, true);
}

void AbstractCharacterBasedApplication::moveCursorLeft()
{
	if (!isShiftModifierPressed() && selection_end && selection_start) { // 選択領域があったら
		if (selection_end != selection_start) {
			SelectionAnchor a = std::min(selection_end, selection_start);
			deselect();
			setCursorRow(a.vrow);
			setCursorCol(a.vcol);
			updateVisibility(true, true, true);
			return;
		}
	}
	
	col_index_t vcol = current_visual_col();
	if (vcol == 0) { // 行頭なら
		if (isSingleLineMode()) {
			// nop
		} else {
			row_index_t vrow = current_visual_row();
			if (vrow > 0) {
				const VisualRowInfo prev_vrow_info = queryVisualRowInfo(vrow - 1);
				const VisualRowInfo curr_vrow_info = queryVisualRowInfo(vrow);
				setCursorRow(vrow - 1); // 上へ移動
				moveCursorEnd(); // 行末へ移動
				if (prev_vrow_info.logical_row == curr_vrow_info.logical_row) { // 同じ論理行の続きなら
					moveCursorLeft(); // 左へ移動
				}
			}
		}
		return;
	}

	setCursorCol(current_visual_col() - 1);
	updateVisibility(true, true, true);
}

void AbstractCharacterBasedApplication::moveCursorRight()
{
	if (!isShiftModifierPressed() && selection_end && selection_start) { // 選択領域があったら
		if (selection_end != selection_start) {
			SelectionAnchor a = std::max(selection_end, selection_start);
			deselect();
			setCursorRow(a.vrow);
			setCursorCol(a.vcol);
			updateVisibility(true, true, true);
			return;
		}
	}

	auto MoveToNextRow = [this](){
		int next_vrow = current_visual_row() + 1;
		if (next_vrow < nlines()) {
			setCursorRow(next_vrow, false);
			moveCursorHome(false); // 行頭へ移動
			return true;
		}
		return false;
	};
	
	auto MoveColumn = [this](col_index_t vcol){
		if (vcol != current_visual_col()) {
			setCursorCol(vcol);
			clearParsedLine();
			updateVisibility(true, true, true);
			return true;
		}
		return false;
	};
	
	const VisualRowInfo curr_vrow_info = queryVisualRowInfo(current_visual_row());
	const VisualRowInfo next_vrow_info = queryVisualRowInfo(current_visual_row() + 1);
	
	std::vector<Character> const &vline = parseCurrentLine(false);
	
	col_index_t vcol = current_visual_col();
	
	char32_t c = -1;
	if (vcol < vline.size()) {
		c = vline[vcol].unicode;
	}
	if (c == '\r' || c == '\n' || c == (char32_t)-1) {
		if (!isSingleLineMode()) {
			MoveToNextRow(); // 次の行の先頭へ移動
		}
		return;
	}
	
	vcol++;
	if (vcol > current_visual_col()) {
		if (curr_vrow_info.logical_row == next_vrow_info.logical_row) { // 同じ論理行の続きなら
			const size_t len = next_vrow_info.logical_col - curr_vrow_info.logical_col; // 物理行の長さ
			if (vcol >= len) { // 行末
				if (MoveToNextRow()) return;
			}
		}
		if (MoveColumn(vcol)) return;
	}
}

void AbstractCharacterBasedApplication::movePageUp()
{
	if (!isSingleLineMode()) {
		int step = editorViewportHeight();
		setCursorRow(current_visual_row() - step);
		setScrollPosRow(scrollpos_row() - step);
		if (current_visual_row() < 0) {
			set_current_visual_row(0);
		}
		if (scrollpos_row() < 0) {
			setScrollPosRow(0);
		}
		clearParsedLine();
		updateVisibility(true, false, true);
	}
}

void AbstractCharacterBasedApplication::movePageDown()
{
	if (!isSingleLineMode()) {
		row_index_t vrow_limit = nlines();
		if (vrow_limit > 0) {
			vrow_limit--;
			int step = editorViewportHeight();
			row_index_t curr_vrow = current_visual_row();
			row_index_t next_vrow = std::min(curr_vrow + step, vrow_limit);
			int scroll_pos = scrollpos_row() + (next_vrow - curr_vrow);
			scroll_pos = std::min(scroll_pos, scrollBottomLimit());
			setCursorRow(next_vrow);
			setScrollPosRow(scroll_pos);
		} else {
			setCursorRow(0);
			setScrollPosRow(0);
		}
		clearParsedLine();
		updateVisibility(true, false, true);
	}
}

/**
 * @brief 現在行の桁座標リストを作成する
 * @param out
 */
void AbstractCharacterBasedApplication::makeColumnPosList(std::vector<int> *out)
{
	out->clear();
	
	std::vector<Character> const &line = parseCurrentLine(false);

	int x = 0;
	while (1) {
		size_t index = out->size();
		out->push_back(x);
		char32_t c = -1;
		if (index < line.size()) {
			c = line.at(index).unicode;
		}
		if (c == '\r' || c == '\n' || c == (uint32_t)-1) {
			break;
		}
		x++;
	}
}

void AbstractCharacterBasedApplication::update_cursor_pos(bool auto_scroll)
{
	if (!cx()->engine) {
		return;
	}

	int index = 0;
	int char_span = 0;
	int col = cx()->current_visual_col_hint;

	{
		std::vector<int> pts;
		makeColumnPosList(&pts);
		if (pts.size() > 1) {
			int newindex = (int)pts.size() - 1;
			for (int i = 0; i + 1 < (int)pts.size(); i++) {
				int x = pts[i];
				if (x <= col && col < pts[i + 1]) {
					char_span = pts[i + 1] - pts[i];
					newindex = i;
					break;
				}
			}
			index = newindex;
		}
	}

	if (char_span < 1) {
		char_span = 1;
	}
	cx()->current_char_span = char_span;

	if (auto_scroll && wrappingMode() == WrappingMode::NoWrap) {
		int pos = decideColumnScrollPos();
		if (scrollpos_col() != pos) {
			setScrollPosCol(pos);
		}
	}
}

void AbstractCharacterBasedApplication::printInvertedBar(int x, int y, char const *text, int padchar)
{
	int w = screenWidth();
	int o = w * y;
	for (int i = 0; i < w; i++) {
		m->screen[o + i].c = 0;
	}

	AbstractCharacterBasedApplication::Option opt;
	opt.char_attr = CharAttr::Invert;
	print(x, y, text, opt);

	for (int i = 0; i < w; i++) {
		if (m->screen[o + i].c == 0) {
			m->screen[o + i].c = padchar;
		}
		m->screen[o + i].a = opt.char_attr;
	}
}

QString AbstractCharacterBasedApplication::statusLine() const
{
	QString text = "[%1:%2]";
	text = text.arg(current_visual_row() + 1).arg(current_visual_col() + 1);
	return text;
}

int AbstractCharacterBasedApplication::printArea(TextEditorContext const *cx, const SelectionAnchor *sel_a, const SelectionAnchor *sel_b)
{
	int end_of_line_y = -1;
	if (cx) {
		int height = cx->viewport_height;
		QRect clip(cx->viewport_org_x, cx->viewport_org_y, cx->viewport_width, height);
		int row = scrollpos_row();
		for (int i = 0; i < height; i++) {
			if (row < 0) continue;
			int y = cx->viewport_org_y + i;
			if (row < (int)nlines()) {
				if (i < height) {
					int x = cx->viewport_org_x - cx->scroll_pos_col;
					Document::Line const *line = this->line(row);
					int anchor_a = -1;
					int anchor_b = -1;
					auto Enabled = [](SelectionAnchor const    *p){ return p && *p; };
					if (Enabled(sel_a) && Enabled(sel_b)) {
						SelectionAnchor a = *sel_a;
						SelectionAnchor b = *sel_b;
						if (a.vrow > b.vrow) {
							std::swap(a, b);
						} else if (a.vrow == b.vrow) {
							if (a.vcol > b.vcol) {
								std::swap(a, b);
							}
						}
						if (row > a.vrow && row < b.vrow) {
							anchor_a = 0;
						} else {
							if (row == a.vrow) {
								anchor_a = a.vcol;
							}
							if (row == b.vrow) {
								anchor_b = b.vcol;
							}
						}
					}
					std::vector<FormattedLine> lines = formatLine_(*line, cx->tab_indent_size, anchor_a, anchor_b);
					for (FormattedLine const &line : lines) {
						AbstractCharacterBasedApplication::Option opt;
						if (line.atts & FormattedLine::StyleID) {
							opt.char_attr.color = QColor(line.atts & 0xff, (line.atts >> 8) & 0xff, (line.atts >> 16) & 0xff);
						}
						opt.clip = clip;
						if (line.isSelected()) {
							opt.char_flag.selected = true;
						}
						x = print(x, y, line.text, opt);
					}
					int end_x = clip.x() + clip.width();
					if (x < end_x) {
						if (x < clip.left()) {
							x = clip.left();
						}
						if (x < end_x) {
							clearRect(x, y, end_x - x, 1);
						}
					}
				}
			} else {
				if (end_of_line_y < 0) {
					end_of_line_y = i;
				}
				clearRect(cx->viewport_org_x, y, cx->viewport_width, 1);
			}
			row++;
		}
	}
	return end_of_line_y;
}

void AbstractCharacterBasedApplication::paintLineNumbers(std::function<void(int, QString const &, Document::Line const *)> const &draw)
{
	auto Line = [&](row_index_t row)-> Document::Line const & {
		return *line(row);
	};

	int rightpadding = 2;
	int left_margin = editor_cx->viewport_org_x;

	for (int i = 0; i <= editor_cx->viewport_height; i++) {
		row_index_t vrow = editor_cx->scroll_pos_row + i;
		auto LineNumberText = [&](int linenum){
			if (linenum > 0) {
				return QString::asprintf("%*u ", left_margin - rightpadding, linenum);
			}
			return QString();
		};
		QString text;
		Document::Line const *line = nullptr;
		if (vrow < (int)nlines()) {
			if (left_margin > 1) {
				line = &Line(vrow);
				unsigned int linenum = 0;
				if (line->sp->meta.line_number_override >= 0) {
					linenum = line->sp->meta.line_number_override;
				} else {
					VisualRowInfo rowinfo = queryVisualRowInfo(vrow); // 物理行から論理行番号を取得する
					if (rowinfo.logical_col == 0) {
						linenum = rowinfo.logical_row + 1;
					}
				}
				if (line->sp->meta.type != Document::LineType::Invalid) {
					text = LineNumberText(linenum);
				}
			}
		} else if (vrow == 0 && nlines() == 0) {
			text = LineNumberText(1);
		}
		int y = editor_cx->viewport_org_y + i;
		draw(y, text, line);
	}
}

bool AbstractCharacterBasedApplication::isAutoLayout() const
{
	return m->auto_layout;
}

void AbstractCharacterBasedApplication::preparePaintScreen()
{
#if 0
	if (m->header_line > 0) {
		char const *line = "Hello, world\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86";
		printInvertedBar(0, 0, line, ' ');
	}
#endif

	if (m->show_line_number) {
		Option opt_normal;
		paintLineNumbers([&](int y, QString const &text, Document::Line const *line){
			(void)line;
			print(0, y, text + '|', opt_normal);
		});
	}

	SelectionAnchor anchor_a;
	SelectionAnchor anchor_b;

	auto MakeSelectionAnchor = [&](){
		if (selection_end) {
			anchor_a = selection_end;
#if 0
			anchor_b.row = cx()->current_row;
			anchor_b.col = cx()->current_col;
			anchor_b.enabled = selection_anchor_0.enabled;
#else
			anchor_b = selection_start;
#endif
		}
	};

	TextEditorContext *cx = editor_cx.get();

	if (isDialogMode()) {
		printArea(cx, &anchor_a, &anchor_b);

		std::string text = m->dialog_title.toStdString();
		text = ' ' + text + ' ';
		int y = screenHeight() - 2;
		printInvertedBar(3, y, text.c_str(), '-');

		MakeSelectionAnchor();
		printArea(dialog_cx.get(), &anchor_a, &anchor_b);
	} else {
		MakeSelectionAnchor();
		cx->bottom_line_y = printArea(cx, &anchor_a, &anchor_b);
	}
}

void AbstractCharacterBasedApplication::onQuit()
{
	if (!m->is_quit_enabled) return;

	if (!isDialogMode()) {
		m->state = State::Exit;
	}
}

void AbstractCharacterBasedApplication::onOpenFile()
{
	if (isReadOnly()) return;
	if (!m->is_open_enabled) return;

	if (!isDialogMode()) {
		execDialog("Open File", recentlyUsedPath(), [&](bool ok, QString const &text){
			if (ok) {
				openFile(text);
			}
		});
	}
}

void AbstractCharacterBasedApplication::onSaveFile()
{
	if (!m->is_save_enabled) return;

	if (!isDialogMode()) {
		execDialog("Save File", recentlyUsedPath(), [&](bool ok, QString const &text){
			if (ok) {
				saveFile(text);
			}
		});
	}
}

void AbstractCharacterBasedApplication::setNormalTextEditorMode(bool f)
{
	m->is_quit_enabled = f;
	m->is_open_enabled = f;
	m->is_save_enabled = f;
	setTerminalMode(!f);
}

SelectionAnchor AbstractCharacterBasedApplication::currentAnchor(bool enabled) const
{
	SelectionAnchor a;
	a.vrow = current_visual_row();
	a.vcol = current_visual_col();
	a.lrow = current_logical_row();
	a.lcol = current_logical_col();
	a.enabled = enabled;
	return a;
}

void AbstractCharacterBasedApplication::setToggleSelectionAnchorEnabled(bool f)
{
	m->is_toggle_selection_anchor_enabled = f;
}

void AbstractCharacterBasedApplication::setReadOnly(bool f)
{
	m->is_read_only = f;
}

bool AbstractCharacterBasedApplication::isReadOnly() const
{
	return m->is_read_only && !m->is_terminal_mode;
}

void AbstractCharacterBasedApplication::setSelectionAnchor(bool enabled, bool update_anchor, bool auto_scroll)
{
	if (update_anchor) {
		selection_end = currentAnchor(enabled);
	} else {
		selection_end.enabled = enabled;
	}
	clearParsedLine();
	updateVisibility(false, false, auto_scroll);
}

void AbstractCharacterBasedApplication::editPaste()
{
	if (isReadOnly()) return;
	if (isTerminalMode()) return;

	setPaintingSuppressed(true);

	QString str = qApp->clipboard()->text();
	utf16(str.utf16(), str.size()).to_utf32([&](uint32_t c){
		write(c, false);
		return true;
	});

	setPaintingSuppressed(false);
	updateVisibility(true, true, true);
}

void AbstractCharacterBasedApplication::editCopy()
{
	edit_(EditOperation::Copy);
}

void AbstractCharacterBasedApplication::editCut()
{
	if (isReadOnly()) return;
	if (isTerminalMode()) return;
	edit_(EditOperation::Cut);
}

void AbstractCharacterBasedApplication::setWriteMode(WriteMode wm)
{
	m->write_mode = wm;
}

bool AbstractCharacterBasedApplication::isInsertMode() const
{
	return m->write_mode == WriteMode::Insert && !isTerminalMode();
}

bool AbstractCharacterBasedApplication::isOverwriteMode() const
{
	return m->write_mode == WriteMode::Overwrite || isTerminalMode();
}

void AbstractCharacterBasedApplication::setTerminalMode(bool f)
{
	m->is_terminal_mode = f;
	if (isTerminalMode()) {
		showLineNumber(false, 0);
		setLineMargin(0);
		setWriteMode(WriteMode::Overwrite);
		setReadOnly(true);
	}
	layoutEditor();
}

bool AbstractCharacterBasedApplication::isTerminalMode() const
{
	return m->is_terminal_mode;
}

void AbstractCharacterBasedApplication::moveToTop()
{
	if (isSingleLineMode()) return;

	deselect();

	set_current_visual_row(0);
	set_current_visual_col(0);
	cx()->current_visual_col_hint = 0;
	setScrollPosRow(0);
	scrollToTop();
	clearParsedLine();
	updateVisibility(true, false, true);
}

void AbstractCharacterBasedApplication::logicalMoveToBottom()
{
	deselect();

	set_current_visual_row(logicalLines());
	set_current_visual_col(0);
	if (current_visual_row() > 0) {
		set_current_visual_row(current_visual_row() - 1);
		clearParsedLine();
		if (Document::Line const *line = currentLine()) {
			int col = calcVisualWidth(Document::Line::View(line->text()));
			set_current_visual_col(col);
			cx()->current_visual_col_hint = col;
		}
	}
	setScrollPosRow(scrollBottomLimit());
}

void AbstractCharacterBasedApplication::logicalMoveToBottom2()
{
	deselect();

	set_current_visual_row(logicalLines());
	set_current_visual_col(0);
	if (current_visual_row() > 0) {
		set_current_visual_row(current_visual_row() - 1);
		clearParsedLine();
		if (Document::Line const *line = currentLine()) {
			int col = calcVisualWidth(Document::Line::View(line->text()));
			set_current_visual_col(col);
			cx()->current_visual_col_hint = col;
		}
	}
	setScrollPosRow(scrollBottomLimit2());
}

void AbstractCharacterBasedApplication::moveToBottom()
{
	if (isSingleLineMode()) return;

	logicalMoveToBottom2();

	clearParsedLine();
	updateVisibility(true, false, true);
}

void AbstractCharacterBasedApplication::internalWrite(const ushort *begin, const ushort *end)
{
	deleteIfSelected();
	clearShiftModifier();
	
	Document *doc = document();
	if (doc->logical_lines.empty()) {
		Document::Line line;
		line.sp->meta.type = Document::LineType::Normal;
		doc->logical_lines.push_back(line);
	}

	if (!isCurrentLineWritable()) return;

	row_index_t vrow = current_visual_row();
	row_index_t lrow = current_logical_row();
	col_index_t lcol = current_logical_col();

	std::vector<Character> vec = parseLogicalLine(cx(), lrow);

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
		invalidateVisualRowInfo(vrow + 1);
	}

	auto [vr, vc] = cx()->line_index_map.logical_to_visual(lrow, lcol);
	setCursorPos(vr, vc);
	updateCurrentPixelX();
	
	updateVisibility(true, true, true);
}

void AbstractCharacterBasedApplication::writeCR()
{
	deleteIfSelected();
	
	moveCursorHome(false);
}

void AbstractCharacterBasedApplication::write(uint32_t c, bool by_keyboard)
{
	if (isTerminalMode()) {
		if (c == '\r') {
			setCursorCol(0);
			clearParsedLine();
			updateVisibility(true, true, true);
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
void AbstractCharacterBasedApplication::appendBulk(std::string_view const &str)
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
			doc->logical_lines.back().append_text(str);
			return;
		}
	}
	
	for (std::string_view line : lines) {
		Document::Line l(std::vector<char>(line.data(), line.data() + line.size()));
		doc->logical_lines.push_back(l);
	}
	
}

void AbstractCharacterBasedApplication::write(char const *ptr, int len, bool by_keyboard)
{
	if (isReadOnly()) return;

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

void AbstractCharacterBasedApplication::write(std::string const &text)
{
	if (!text.empty()) {
		write(text.c_str(), (int)text.size(), false);
	}
}

void AbstractCharacterBasedApplication::write_(char const *ptr, bool by_keyboard)
{
	write(ptr, -1, by_keyboard);
}

void AbstractCharacterBasedApplication::write_(QString const &text, bool by_keyboard)
{
	if (isReadOnly()) return;

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

void AbstractCharacterBasedApplication::write(QKeyEvent *e)
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

