#ifndef ABSTRACTTEXTEDITORAPPLICATION_H
#define ABSTRACTTEXTEDITORAPPLICATION_H

#include <QByteArray>
#include <QColor>
#include <QKeyEvent>
#include <QRect>
#include <QString>
#include <functional>
#include <memory>
#include <optional>
#include <variant>
#include <vector>
#include <mutex>
#include <QFont>
#include <QPixmap>
#include <QPainter>
#include <QFontMetrics>

#include "LineIndexMap/LineIndexMap.h"

namespace EscapeCode {
enum EscapeCode {
	Up = 0x1b5b4100,
	Down = 0x1b5b4200,
	Right = 0x1b5b4300,
	Left = 0x1b5b4400,
	Home = 0x1b4f4800,
	End = 0x1b4f4600,
	Insert = 0x1b5b327e,
	Delete = 0x1b5b337e,
	PageUp = 0x1b5b357e,
	PageDown = 0x1b5b367e,
};
}

struct CharAttr {
	enum Index {
		Normal,
		Invert,
		Hilite,
	};
	uint16_t index = 0;
	QColor color;
	CharAttr(int index = Normal)
		: index(index)
	{
	}
	bool operator == (CharAttr const &r) const
	{
		return index == r.index && color == r.color;
	}
	bool operator != (CharAttr const &r) const
	{
		return !operator == (r);
	}
};

struct CharFlags {
	enum DiffMarker {
		Undefined,
		Del,
		Add,
		_Reserved
	};
	union {
		struct {
			bool selected : 1;
			bool current_line : 1;
			uint8_t diff_marker : 2;
			
		};
		uint16_t all = 0;
	};
};

struct Character {
	char32_t unicode = 0;
	int left_x = 0;
	int right_x = 0;
	CharAttr attr;
	Character() = default;
	Character(char32_t unicode)
		: unicode(unicode)
	{
	}
	operator char32_t () const
	{
		return unicode;
	}
};
class CharBuffer {
public:
	std::shared_ptr<std::vector<Character>> vec;
	CharBuffer()
		: vec(std::make_shared<std::vector<Character>>())
	{
	}
	CharBuffer(std::vector<Character> const &v)
		: vec(std::make_shared<std::vector<Character>>(v))
	{
	}
	void reserve(size_t n)
	{
		vec->reserve(n);
	}
	void clear()
	{
		vec->clear();
	}
	void resize(size_t n)
	{
		vec->resize(n);
	}
	size_t size() const
	{
		return vec->size();
	}
	bool empty() const
	{
		return vec->empty();
	}
	Character const &back() const
	{
		return vec->back();
	}
	Character &operator [] (size_t i)
	{
		return (*vec)[i];
	}
	Character const &operator [] (size_t i) const
	{
		return (*vec)[i];
	}
	void push_back(Character const &c)
	{
		vec->push_back(c);
	}
	void emplace_back(Character const &c)
	{
		vec->emplace_back(c);
	}
	std::vector<Character>::iterator begin()
	{
		return vec->begin();
	}
	std::vector<Character> ::iterator end()
	{
		return vec->end();
	}
	void insert(std::vector<Character>::iterator pos, Character const &first)
	{
		vec->insert(pos, first);
	}
	void insert(std::vector<Character>::iterator pos, Character const *first, Character const *last)
	{
		vec->insert(pos, first, last);
	}
	void insert(std::vector<Character>::iterator pos, std::vector<Character>::const_iterator first, std::vector<Character>::const_iterator last)
	{
		vec->insert(pos, first, last);
	}
	void erase(std::vector<Character>::iterator pos)
	{
		vec->erase(pos);
	}
	void erase(std::vector<Character>::iterator first, std::vector<Character>::iterator last)
	{
		vec->erase(first, last);
	}
};

typedef int32_t row_index_t;
typedef int32_t col_index_t;

class Document {
public:
	typedef std::variant<std::vector<char>, std::string_view> varline_t;
	
	struct LineProperty {
		CharBuffer chars;
		std::vector<CharFlags> flags;
		bool char_diff = false;
	};
	
	enum LineType {
		Invalid,
		Normal,
		Add,
		Del,
	};
	struct Line {
		struct Meta {
			LineType type = Normal;
			col_index_t logical_col_pos = 0;
			col_index_t logical_col_len = 0;
			int32_t line_number_override = -1;
			mutable std::shared_ptr<LineProperty> detail;
			mutable std::vector<Document::Line> visual_lines;
		};
		struct D {
			varline_t text = std::string_view();
			Meta meta;
		};
		std::shared_ptr<D> sp;
		
		Line()
			: sp(std::make_shared<D>())
		{
		}
		
		explicit Line(std::vector<char> const &ba, LineType type = Normal)
			: sp(std::make_shared<D>())
		{
			sp->text = ba;
			sp->meta.type = type;
		}
		
		explicit Line(QByteArray const &ba, LineType type = Normal)
			: sp(std::make_shared<D>())
		{
			sp->text = std::vector<char>(ba.data(), ba.data() + ba.size());
			sp->meta.type = type;
		}
	
		static Line InvalidLine()
		{
			Line line;
			line.sp->meta.type = Invalid;
			return line;
		}
		
		static Line NormalEmptyLine()
		{
			Line line;
			line.sp->meta.type = Normal;
			return line;
		}
		
		static Line None()
		{
			Line line;
			line.sp->meta.type = Invalid;
			return line;
		}
		
		static Line View(std::string_view v, LineType type)
		{
			Line line;
			line.sp->text = v;
			line.sp->meta.type = type;
			return line;
		}
		
		static Line View(std::string_view v, Meta const &meta)
		{
			Line line;
			line.sp->text = v;
			line.sp->meta = meta;
			return line;
		}
		
		static Line View(std::string_view v)
		{
			return View(v, {});
		}
		
		static Line View(Line const &line)
		{
			if (std::holds_alternative<std::string_view>(line.sp->text)) {
				return line;
			}
			std::vector<char> const *v = std::get_if<std::vector<char>>(&line.sp->text);
			assert(v);
			return View(std::string_view(v->data(), v->size()), line.sp->meta);
		}
		
		LineType type() const
		{
			return sp->meta.type;
		}
		
		void set_line_number_override(int32_t num)
		{
			sp->meta.line_number_override = num;
		}
		
		LineProperty *detail() const
		{
			return sp->meta.detail.get();
		}
		
		LineProperty *newDetail()
		{
			sp->meta.detail = std::make_shared<LineProperty>();
			return detail();
		}
		
		void clearDetail()
		{
			sp->meta.detail.reset();
		}
		
		bool endsWithNewLine() const
		{
			int c = text().empty() ? 0 : text().back();
			return c == '\n' || c == '\r';
		}
		
		std::string_view text() const
		{
			if (std::holds_alternative<std::string_view>(sp->text)) {
				return std::get<std::string_view>(sp->text);
			}
			std::vector<char> const *v = std::get_if<std::vector<char>>(&sp->text);
			assert(v);
			return std::string_view(v->data(), v->size());
		}
		
		void set_text(std::vector<char> const &text)
		{
			sp->text = text;
		}
		
		std::vector<char> *to_vector()
		{
			if (std::string_view *sv = std::get_if<std::string_view>(&sp->text)) {
				sp->text = std::vector<char>(sv->data(), sv->data() + sv->size());
			}
			std::vector<char> *v = std::get_if<std::vector<char>>(&sp->text);
			assert(v);
			return v;
		}
		
		void append_text(std::string_view new_text)
		{
			if (!new_text.empty()) {
				std::vector<char> *v = to_vector();
				v->insert(v->end(), new_text.data(), new_text.data() + new_text.size());
			}
		}
		
		void append_text(const std::vector<char> &new_text)
		{
			if (!new_text.empty()) {
				append_text(std::string_view(new_text.data(), new_text.size()));
			}
		}
		
		void append_text(char c)
		{
			append_text(std::string_view(&c, 1));
		}
		
		void clear_visual_lines()
		{
			sp->meta.visual_lines.clear();
		}
	};
	
	
	QByteArray all;
	std::vector<varline_t> raw_lines;

	std::vector<Line> logical_lines;
};

class TextEditorEngine {
public:
	Document document;
};

struct LogicalRowInfo {
	row_index_t visual_row = 0;
};

struct VisualRowInfo {
	row_index_t lrow = 0;
	col_index_t lcol = 0;
};

struct SelectionAnchor {
	bool enabled = false;
	row_index_t lrow = 0;
	col_index_t lcol = 0;
	explicit operator bool () const
	{
		return enabled;
	}
	int compare(SelectionAnchor const &a) const
	{
		if (enabled && a.enabled) {
			if (lrow < a.lrow) return -1;
			if (lrow > a.lrow) return 1;
			if (lcol < a.lcol) return -1;
			if (lcol > a.lcol) return 1;
		} else {
			if (a.enabled) return -1;
			if (enabled) return 1;
		}
		return 0;
	}
};
static inline bool operator == (SelectionAnchor const &a, SelectionAnchor const &b) { return a.compare(b) == 0; }
static inline bool operator != (SelectionAnchor const &a, SelectionAnchor const &b) { return a.compare(b) != 0; }
static inline bool operator <= (SelectionAnchor const &a, SelectionAnchor const &b) { return a.compare(b) <= 0; }
static inline bool operator >= (SelectionAnchor const &a, SelectionAnchor const &b) { return a.compare(b) >= 0; }
static inline bool operator < (SelectionAnchor const &a, SelectionAnchor const &b) { return a.compare(b) < 0; }
static inline bool operator > (SelectionAnchor const &a, SelectionAnchor const &b) { return a.compare(b) > 0; }

using TextEditorEngine_sp = std::shared_ptr<TextEditorEngine>;

struct TextEditorContext {
	QRect cursor_rect;
	row_index_t current_visual_row = 0; // 表示行（物理行）
	col_index_t current_visual_col = 0; // 表示列（物理列）
	int current_visual_col_hint = 0;
	int current_absolute_x_px = 0; // 桁ピクセル座標（行頭基準）
	int current_visual_x_px = 0; // 桁ピクセル座標（クライアント領域基準）
	int current_visual_y_px = 0; // 行ピクセル座標
	row_index_t saved_row = 0;
	col_index_t saved_col = 0;
	int saved_col_hint = 0;
	int current_char_span = 1;
	int scroll_horz_pos_px = 0;
	int scroll_vert_pos_px = 0;
	col_index_t viewport_org_x_cols = 0; // テキスト領域の原点（桁位置）（行番号表示領域の幅の文字数）
	row_index_t viewport_org_y_rows = 0;
	int viewport_width_px = 640;
	int viewport_height_rows = 25;
	int tab_indent_size = 4;
	int bottom_line_y = -1;
	TextEditorEngine_sp engine;
	LineIndexMap line_index_map;
	struct Cache {
		std::optional<row_index_t> nlines;
		std::vector<Document::Line> visual_lines;
		row_index_t current_logical_row = 0;
		col_index_t current_logical_col = 0;
		bool scroll_bar_update_needed = true;
	};
	mutable Cache cache;
};

struct RowCol {
	row_index_t row = 0;
	col_index_t col = 0;
	RowCol(row_index_t row = 0, col_index_t col = 0)
		: row(row)
		, col(col)
	{
	}
};

class AbstractTextEditorApplication {
public:
	class Font {
	public:
		struct TextWidthCache {
			std::unordered_map<QString, int> map;
		};
		QFont text_font_;
		std::unique_ptr<QFontMetrics> fm_;
		int ascent_ = 0;
		int descent_ = 0;
		QSize basic_character_size_;
		mutable TextWidthCache text_width_cache_;

		void set_font(QFont const &font)
		{
			text_font_ = font;

			QPixmap pm(1, 1);
			QPainter pr(&pm);
			pr.setFont(text_font_);
			fm_ = std::make_unique<QFontMetrics>(pr.fontMetrics());
			ascent_ = fm_->ascent();
			descent_ = fm_->descent();
			basic_character_size_ = QSize(fm_->horizontalAdvance("0"), fm_->height());
		}
		QFont font() const
		{
			return text_font_;
		}
		int basis_char_width() const
		{
			int w = basic_character_size_.width();
			return w > 0 ? w : 1;
		}
		int basis_char_height() const
		{
			int h = basic_character_size_.height();
			return h > 0 ? h : 1;
		}
		int text_width(QString const &text) const
		{
			int ret = 0;
			auto it = text_width_cache_.map.find(text);
			if (it != text_width_cache_.map.end()) {
				ret = it->second;
			} else {
				ret = fm_->horizontalAdvance(text);
				text_width_cache_.map[text] = ret;
			}
			return ret;
		}
	};

	static const int LINE_NUMBER_AREA_WIDTH = 8;
	
	enum class WriteMode {
		Insert,
		Overwrite,
	};
	
	enum class State {
		Normal,
		Exit,
	};
	
	enum class WrappingMode {
		NoWrap,
		CharWrap,
		WordWrap,
	};
	
	struct Option {
		CharAttr char_attr = {};
		CharFlags char_flag = {};
		QRect clip;
	};
	
	struct Char16 {
		uint16_t c = 0;
		CharAttr a;
	};
	
	static int charWidth(uint32_t c);
	
	class FormattedLine {
	public:
		QString text;
		enum Attr {
			StyleID = 0x00ffffff,
			Selected = 0x01000000,
		};
		uint32_t atts;
		FormattedLine(QString const &text, int atts)
			: text(text)
			, atts(atts)
		{
		}
		bool isSelected() const
		{
			return atts & Selected;
		}
	};
	
private:
	struct Private;
	Private *m;
protected:
	SelectionAnchor const &selection_start() const;
	SelectionAnchor const &selection_end() const;
	void set_selection_start(SelectionAnchor const &anchor);
	void set_selection_end(SelectionAnchor const &anchor);
	void set_selection_start_enabled(bool enabled);
	void set_selection_end_enabled(bool enabled);
	void sync_selection();
	void clear_selection();
protected:

	row_index_t visual_nlines() const;
	void invalidate_nlines_cache();

	Document::Line *visual_line(row_index_t vrow);
	
	Document::Line const *visual_line(row_index_t vrow) const
	{
		return const_cast<AbstractTextEditorApplication *>(this)->visual_line(vrow);
	}
	
	void initEditor();
protected:
	const Document::Line *currentLine() const;
	void clearParsedLine();
	
	void set_current_visual_row(row_index_t row);
	void set_current_visual_col(col_index_t col);
	row_index_t current_visual_row() const;
	int current_visual_col() const;
	int current_visual_x_px() const;
	
	int scroll_vert_pos_px() const;
	int scroll_horz_pos_px() const;

	int cursor_col_px() const;
	int cursor_row_px() const;

	void set_scroll_vert_pos_px(int row);
	void set_scroll_horz_pos_px(int col);
	
	int editor_viewport_width_px() const;
	int editor_viewport_height() const;
	
	std::shared_ptr<TextEditorContext> editor_cx;
	
	TextEditorContext *cx();
	TextEditorContext const *cx() const;
	
	Document *document();
	Document const *document() const;
	int logical_nlines() const;
	
	void ensureCurrentLineVisible();
	
	int leftMargin_() const;
	
	struct UpdateVisibilityOption {
		bool ensure_current_line_visible = true;
		bool change_col = true;
		bool auto_scroll = true;
	};
	virtual void updateVisibility(UpdateVisibilityOption const &arg) = 0;
	
	void insert_line(row_index_t lrow);
	bool commit_line(row_index_t lrow, const CharBuffer &vec);
	
	void doDelete();
	void doBackspace();
	
	void invalidate_visual_row_info(row_index_t vrow, size_t n = -1);
	void invalidate_logical_row_info(row_index_t vrow);
	void erase_parsed_line_cache(row_index_t vrow, size_t n = -1);

	LineIndexMap::LogicalPosition query_logical_for_visual_row(row_index_t vrow);

	virtual void calc_pos_x(CharBuffer *chars) const {}
	
private:
	void internalWrite(const ushort *begin, const ushort *end);
	SelectionAnchor currentAnchor(bool enabled) const;
	enum class EditOperation {
		Cut,
		Copy,
	};
	void edit_selection(EditOperation op, CharBuffer *clip_text_out);
	int calcColumnToIndex(int column);
	void _edit_op(EditOperation op);
	bool isCurrentLineWritable() const;
	void initEngine(const std::shared_ptr<TextEditorContext>& cx);
	void writeCR();
	bool deleteIfSelected();
	void _set_cursor_col(col_index_t col, bool auto_scroll = true, bool by_mouse = false);
	std::vector<Document::Line> *documentLinesForWrite(bool check_readonly = true);
public:
	row_index_t lrow_to_vrow(row_index_t lrow) const;
	row_index_t vrow_to_lrow(row_index_t vrow) const;
	RowCol visual_position(SelectionAnchor const &a) const;
protected:
	CharBuffer parseLogicalLine(const TextEditorContext *cx, row_index_t lrow) const;
	const CharBuffer *parseCurrentLine() const;
private:
	CharBuffer _parseLine(const TextEditorContext *cx, const Document::Line *line, std::mutex *mutex) const;
protected:
	CharBuffer _parseLine(Document::Line const *line, std::mutex *mutex = nullptr) const;
	CharBuffer *parseLine(row_index_t vrow) const;

	virtual void updateScrollBarRange() {}
	
	virtual void setCursorRow(row_index_t vrow, bool auto_scroll = true, bool by_mouse = false);
	virtual void setCursorCol(col_index_t vcol);
	void setCursorPos(RowCol const &vpos);
	void setCursorPosByMouse(RowCol vpos, QPoint pt);
	
	static int nextTabStop(TextEditorContext const *cx, int x);
	int scrollBottomLimit() const;
	int scrollBottomLimit2() const;
	bool isPaintingSuppressed() const;
	void setPaintingSuppressed(bool f);
	
	void writeNewLine();
	
	void update_horz_scroll();
	
	QString statusLine() const;
	
	
	void paintLineNumbers(std::function<void(int, QString const &, Document::Line const *)> const &draw);
	bool isAutoLayout() const;
	void savePos();
	void restorePos();
public:
	
	AbstractTextEditorApplication();
	virtual ~AbstractTextEditorApplication();

	void setRecentlyUsedPath(QString const &path);
	QString recentlyUsedPath();
	
	virtual void layoutEditor();
	void scrollUp();
	void scrollDown();
	void moveCursorOut();
	void moveCursorHome(bool consider_indent);
	void moveCursorEnd();
	void moveCursorUp();
	virtual void moveCursorDown();
	void moveCursorLeft();
	void moveCursorRight();
	void movePageUp();
	void movePageDown();
	void scrollToTop();
	
	TextEditorEngine_sp engine() const;
	int client_width_px() const;
	int client_height_px() const;
	void set_client_size(int w, int h, bool update_layout);
	void setContentWidth(int w);
	void setTextEditorEngine(const TextEditorEngine_sp &e);
	bool openFile(QString const &path);
	void saveFile(QString const &path);
	void loadExampleFile();
	void pressEnter();
	void pressEscape();
	State state() const;
	bool isLineNumberVisible() const;
	void showLineNumber(bool show, int left_margin = LINE_NUMBER_AREA_WIDTH);
	void set_auto_layout(bool f);
	void setDocument(const std::vector<Document::Line> *source);
	void setSelectionAnchor(bool enabled, bool update_anchor, bool auto_scroll);
	void setNormalTextEditorMode(bool f);
	void set_read_only(bool f);
	bool is_read_only() const;
	void editPaste();
	void edit_copy();
	void edit_cut();
	void setWriteMode(WriteMode wm);
	bool isInsertMode() const;
	bool isOverwriteMode() const;
	void setTerminalMode(bool f);
	bool isTerminalMode() const;
	void moveToTop();
	void moveToBottom();
	void setLineMargin(int n);
	void write(uint32_t c, bool by_keyboard);
	void write(char const *ptr, int len, bool by_keyboard);
	void write(std::string const &text);
	void write(QKeyEvent *e);
	void setCursorVisible(bool show);
	bool isCursorVisible();
	void setModifierKeys(Qt::KeyboardModifiers const &keymod);
	bool isControlModifierPressed() const;
	bool isShiftModifierPressed() const;
	void clearShiftModifier();
	bool isChanged() const;
	void setChanged(bool f);
	void logicalMoveToBottom();
	void appendBulk(std::string_view const &str);
	void clear();
private:
	std::vector<Document::Line> wrap_line(Document::Line line, std::mutex *mutex) const;
	void _update_visual_line_by_logical_line(col_index_t lrow, Document::Line const &ll, std::mutex *mutex);

	void _wrap_line(Document::Line *ll, bool force, std::mutex *mutex);
	void _update_line_index_map(row_index_t lrow, Document::Line *ll, std::mutex *mutex);

	bool _update_line(row_index_t lrow, std::optional<std::vector<char>> text, bool force, std::mutex *mutex);
protected:
	bool update_visual_line(row_index_t lrow, bool force);
	void update_visual_lines_all();
private:
	void _update_logical_pos_cache() const;
	void delete_line(row_index_t lrow);
protected:
	row_index_t current_logical_row() const;
	col_index_t current_logical_col() const;
protected:
	bool isWidthFixed() const;
protected:
	void write_(char const *ptr, bool by_keyboard);
	void write_(QString const &text, bool by_keyboard);
	
	bool hasSelection() const;
	void updateSelectionAnchor1(bool auto_scroll);
	void updateSelectionAnchor2(bool auto_scroll);
	virtual std::pair<int, int> currentPixelX() const { return {}; }
	
	void setFixedFont(const QFont &font);
	void setTextFont(const QFont &font);
	Font const &fixedFontMetrics() const;
	Font const &textFontMetrics() const;
	void set_line_margin_px(int top, int bottom);	
	int line_baseline_px() const;
	void need_to_update_scroll_bar();
	int linenum_area_width_px() const;
	void new_document();
	void invalidateParsedLineByLogicalRow(row_index_t lrow);
public:
	int line_height_px() const;
	
	void setWrappingMode(WrappingMode mode);
	AbstractTextEditorApplication::WrappingMode wrappingMode() const;

	bool save(std::function<bool (char const *p, size_t n)> callback) const
	{
		std::vector<Document::Line> const &llines = document()->logical_lines;
		for (Document::Line const &line : llines) {
			std::string_view view = line.text();
			if (!callback(view.data(), view.size())) {
				return false;
			}
		}
		return true;
	}
};

#endif // ABSTRACTTEXTEDITORAPPLICATION_H
