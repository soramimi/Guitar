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

#include "LineIndexMap/LineIndexMap.h"

class AbstractTextMetrics {
public:
	virtual int basisCharWidth() const = 0;
	virtual int textWidth(QString const &text) const = 0;
};

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

typedef int32_t row_index_t;
typedef int32_t col_index_t;

class Document {
public:
	typedef std::variant<std::vector<char>, std::string_view> varline_t;
	
	struct LineProperty {
		std::vector<Character> chars;
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
	// bool single_line = false;
	row_index_t current_visual_row = 0; // 表示行（物理行）
	col_index_t current_visual_col = 0; // 表示列（物理列）
	int current_visual_col_hint = 0;
	int current_visual_pixel_x = 0; // 桁ピクセル座標
	int current_visual_pixel_y = 0; // 行ピクセル座標
	row_index_t saved_row = 0;
	col_index_t saved_col = 0;
	int saved_col_hint = 0;
	int current_char_span = 1;
	col_index_t scroll_horz_pos = 0;
	row_index_t scroll_vert_pos = 0;
	col_index_t viewport_org_x = 0;
	row_index_t viewport_org_y = 0;
	int viewport_width = 80;
	int viewport_height = 23;
	int tab_indent_size = 4;
	int bottom_line_y = -1;
	TextEditorEngine_sp engine;
	LineIndexMap line_index_map;
	struct Cache {
		std::optional<row_index_t> nlines;
		std::vector<Document::Line> visual_lines;
		row_index_t current_logical_row = 0;
		col_index_t current_logical_col = 0;
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
	static const int LEFT_MARGIN = 8;
	static const int RIGHT_MARGIN = 10;
	
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

	row_index_t nlines() const;
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
	int current_visual_pixel_x() const;
	
	int scroll_vert_pos() const;
	int scroll_horz_pos() const;

	int cursor_col() const;
	int cursor_row() const;

	void set_scroll_vert_pos(int row);
	void set_scroll_horz_pos(int col);
	
	int editor_viewport_width() const;
	int editor_viewport_height() const;
	
	virtual int print(int x, int y, QString const &text, Option const &opt);
	
	std::shared_ptr<TextEditorContext> editor_cx;
	
	TextEditorContext *cx();
	TextEditorContext const *cx() const;
	
	Document *document();
	Document const *document() const;
	int logicalLines() const;
	
	void ensureCurrentLineVisible();
	
	// int calcVisualWidth(Document::Line const &line) const;
	
	int leftMargin_() const;
	
	void makeBuffer();
	
	struct UpdateVisibilityOption {
		bool ensure_current_line_visible = true;
		bool change_col = true;
		bool auto_scroll = true;
	};
	virtual void updateVisibility(UpdateVisibilityOption const &arg) = 0;
	
	void insertLine(row_index_t lrow);
	bool commit_line(row_index_t lrow, const std::vector<Character> &vec);
	
	void doDelete();
	void doBackspace();
	
	void invalidate_visual_row_info(row_index_t vrow);

	LineIndexMap::LogicalPosition query_logical_for_visual_row(row_index_t vrow);

	virtual void calc_pos_x(std::vector<Character> *chars) const {}
	
private:
	void internalWrite(const ushort *begin, const ushort *end);
	void printInvertedBar(int x, int y, char const *text, int padchar);
	SelectionAnchor currentAnchor(bool enabled) const;
	enum class EditOperation {
		Cut,
		Copy,
	};
	void edit_selection(EditOperation op, std::vector<Character> *clip_text_out);
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
	std::vector<Character> parseLogicalLine(const TextEditorContext *cx, row_index_t lrow) const;
	const std::vector<Character> &parseCurrentLine() const;
private:
	std::vector<Character> _parseLine(const TextEditorContext *cx, const Document::Line *line, std::mutex *mutex) const;
protected:
	std::vector<Character> parseLine(Document::Line const *line, std::mutex *mutex = nullptr) const;
	std::vector<Character> parseLine(row_index_t vrow) const;

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
	
	void setRecentlyUsedPath(QString const &path);
	QString recentlyUsedPath();
	void clearRect(int x, int y, int w, int h);
	void paintLineNumbers(std::function<void(int, QString const &, Document::Line const *)> const &draw);
	bool isAutoLayout() const;
	void savePos();
	void restorePos();
public:
	
	AbstractTextEditorApplication();
	virtual ~AbstractTextEditorApplication();
	
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
	int screenWidth() const;
	int screenHeight() const;
	void setScreenSize(int w, int h, bool update_layout);
	void setContentWidth(int w);
	void setTextEditorEngine(const TextEditorEngine_sp &e);
	void openFile(QString const &path);
	void saveFile(QString const &path);
	void loadExampleFile();
	void pressEnter();
	void pressEscape();
	State state() const;
	bool isLineNumberVisible() const;
	void showLineNumber(bool show, int left_margin = LEFT_MARGIN);
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
	// void makeColumnPosList(std::vector<int> *out) const;
	bool hasSelection() const;
	void updateSelectionAnchor1(bool auto_scroll);
	void updateSelectionAnchor2(bool auto_scroll);
	virtual int currentPixelX() const { return 0; }
public:
	void setWrappingMode(WrappingMode mode);
	AbstractTextEditorApplication::WrappingMode wrappingMode() const;
};

#endif // ABSTRACTTEXTEDITORAPPLICATION_H
