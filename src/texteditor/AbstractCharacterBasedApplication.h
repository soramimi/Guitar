#ifndef ABSTRACTCHARACTERBASEDAPPLICATION_H
#define ABSTRACTCHARACTERBASEDAPPLICATION_H

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

#include "somethingmap.h"

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
		
		void set_text(const std::vector<char> &new_text)
		{
			sp->text = new_text;
			sp->meta.detail.reset();
			clear_visual_lines();
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
	row_index_t logical_row = 0;
	col_index_t logical_col = 0;
};

struct SelectionAnchor {
	enum Enabled {
		False,
		True,
	};
	
	Enabled enabled = False;
	row_index_t row = 0;
	col_index_t col = 0;
	int compare(SelectionAnchor const &a) const
	{
		if (enabled && a.enabled) {
			if (row < a.row) return -1;
			if (row > a.row) return 1;
			if (col < a.col) return -1;
			if (col > a.col) return 1;
		} else {
			if (a.enabled) return -1;
			if (enabled) return 1;
		}
		return 0;
	}
	bool operator == (SelectionAnchor const &a) const
	{
		return compare(a) == 0;
	}
	bool operator != (SelectionAnchor const &a) const
	{
		return compare(a) != 0;
	}
	bool operator < (SelectionAnchor const &a) const
	{
		return compare(a) < 0;
	}
	bool operator > (SelectionAnchor const &a) const
	{
		return compare(a) > 0;
	}
	bool operator <= (SelectionAnchor const &a) const
	{
		return compare(a) <= 0;
	}
	bool operator >= (SelectionAnchor const &a) const
	{
		return compare(a) >= 0;
	}
};

using TextEditorEngine_sp = std::shared_ptr<TextEditorEngine>;

struct TextEditorContext {
	QRect cursor_rect;
	bool single_line = false;
	row_index_t current_visual_row = 0; // a.k.a. physical row
	col_index_t current_visual_col = 0; // physical column
	int current_visual_col_hint = 0;
	int current_visual_pixel_x = 0; // 桁ピクセル座標
	int current_visual_pixel_y = 0; // 行ピクセル座標
	row_index_t saved_row = 0;
	col_index_t saved_col = 0;
	int saved_col_hint = 0;
	int current_char_span = 1;
	int scroll_pos_row = 0;
	int scroll_pos_col = 0;
	int viewport_org_x = 0;
	int viewport_org_y = 1;
	int viewport_width = 80;
	int viewport_height = 23;
	int tab_indent_size = 4;
	int bottom_line_y = -1;
	TextEditorEngine_sp engine;
	std::vector<LogicalRowInfo> logical_row_info; // 論理行から物理行へのマッピング情報
	std::vector<VisualRowInfo> visual_row_info; // 物理行から論理行へのマッピング情報
	std::vector<Document::Line> visual_lines;
	SomethingMap something_map;
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

using DialogHandler = std::function<void (bool, QString const &)>;

class AbstractCharacterBasedApplication {
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
	
	enum LineFlag {
		LineChanged = 1,
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
	
	std::vector<FormattedLine> formatLine_(const Document::Line &line, int tab_indent_size, int anchor_a = -1, int anchor_b = -1) const;
	
private:
	struct Private;
	Private *m;
protected:
	SelectionAnchor selection_start;
	SelectionAnchor selection_end;
protected:

	std::vector<Document::Line> *_lines();
	
	std::vector<Document::Line> const *_lines() const
	{
		return const_cast<AbstractCharacterBasedApplication *>(this)->_lines();
	}
	
	row_index_t nlines() const;

	Document::Line *line(row_index_t vrow);
	
	Document::Line const *line(row_index_t vrow) const
	{
		return const_cast<AbstractCharacterBasedApplication *>(this)->line(vrow);
	}
	
	int char_screen_w() const;
	int char_screen_h() const;
	std::vector<Char16> *char_screen();
	std::vector<Char16> const *char_screen() const;
	std::vector<uint8_t> *line_flags();
	
	void initEditor();
protected:
        const Document::Line *currentLine() const;
	void clearParsedLine();
	
	int currentVisualPixelX() const;
	void setCurrentVisualRow(row_index_t row);
	void setCurrentVisualCol(int col);
	
	int scrollposRow() const;
	int scrollposCol() const;

	int cursorCol() const;
	int cursorRow() const;

	void setScrollPosRow(int row);
	void setScrollPosCol(int col);
	
	int editorViewportWidth() const;
	int editorViewportHeight() const;
	
	virtual int print(int x, int y, QString const &text, Option const &opt);
	
	std::shared_ptr<TextEditorContext> editor_cx;
	std::shared_ptr<TextEditorContext> dialog_cx;
	
	TextEditorContext *cx();
	TextEditorContext const *cx() const;
	
	Document *document();
	Document const *document() const;
	int logicalLines() const;
	
	bool isSingleLineMode() const;
	
	void ensureCurrentLineVisible();
	int decideColumnScrollPos() const;
	
	int calcVisualWidth(Document::Line const &line) const;
	
	int leftMargin_() const;
	
	void makeBuffer();
	int printArea(const TextEditorContext *cx, SelectionAnchor const *sel_a = nullptr, SelectionAnchor const *sel_b = nullptr);
	
	virtual void updateVisibility(bool ensure_current_line_visible, bool change_col, bool auto_scroll) = 0;
	
	void insertLine(row_index_t lrow);
	bool commitLine(const std::vector<Character> &vec);
	
	void doDelete();
	void doBackspace();
	
	bool isDialogMode();
	void setDialogMode(bool f);
	void closeDialog(bool result);
	void setDialogOption(QString const &title, QString const &value, const DialogHandler &handler);
	void execDialog(QString const &dialog_title, const QString &dialog_value, const DialogHandler &handler);
	
	void invalidateVisualRowInfo(row_index_t vrow);
	void _reserveVisualRowInfo(row_index_t vrow);
	VisualRowInfo queryVisualRowInfo(row_index_t vrow);
	void upadteVisualRow(row_index_t vrow);

	virtual void calc_pos_x(std::vector<Character> *chars) const {}
	
private:
	void internalWrite(const ushort *begin, const ushort *end);
	void pressLetterWithControl(int c);
	void invalidateAreaBelowTheCurrentLine();
	void onQuit();
	void onOpenFile();
	void onSaveFile();
	void printInvertedBar(int x, int y, char const *text, int padchar);
	SelectionAnchor currentAnchor(SelectionAnchor::Enabled enabled);
	enum class EditOperation {
		Cut,
		Copy,
	};
	void editSelected(EditOperation op, std::vector<Character> *cutbuffer);
	int calcColumnToIndex(int column);
	void edit_(EditOperation op);
	bool isCurrentLineWritable() const;
	void initEngine(const std::shared_ptr<TextEditorContext>& cx);
	void writeCR();
	bool deleteIfSelected();
	void setCursorCol_(col_index_t col, bool auto_scroll = true, bool by_mouse = false);
	std::vector<Document::Line> *documentLinesForWrite(bool check_readonly = true);
	row_index_t lrow_to_vrow(row_index_t lrow);
protected:
	void deselect();
	std::vector<Character> parseLogicalLine(const TextEditorContext *cx, row_index_t lrow) const;
	const std::vector<Character> &parseCurrentLine(bool force);
	std::vector<Character> _parseLine(const TextEditorContext *cx, const Document::Line *line, std::mutex *mutex) const;
	std::vector<Character> parseLine(Document::Line const *line, std::mutex *mutex = nullptr) const;
	std::vector<Character> parseLine(row_index_t vrow) const;

	virtual void updateScrollBarRange() {}
	
	virtual void setCursorRow(row_index_t vrow, bool auto_scroll = true, bool by_mouse = false);
	virtual void setCursorCol(col_index_t vcol)
	{
		setCursorCol_(vcol, true, false);
	}
	void setCursorPosByMouse(RowCol vpos, QPoint pt)
	{
		setCursorRow(vpos.row, false, true);
		setCursorCol_(vpos.col, false, true);
		cx()->current_visual_pixel_x = pt.x();
	}
	void setCursorPos(int row, int col)
	{
		setCursorRow(row, false);
		setCursorCol_(col, false, false);
	}
	static int nextTabStop(TextEditorContext const *cx, int x);
	int scrollBottomLimit() const;
	int scrollBottomLimit2() const;
	bool isPaintingSuppressed() const;
	void setPaintingSuppressed(bool f);
	
	void writeNewLine();
	
	void updateCursorPos(bool auto_scroll);
	
	QString statusLine() const;
	
	void preparePaintScreen();
	void setRecentlyUsedPath(QString const &path);
	QString recentlyUsedPath();
	void clearRect(int x, int y, int w, int h);
	void paintLineNumbers(std::function<void(int, QString const &, Document::Line const *)> const &draw);
	bool isAutoLayout() const;
	void invalidateArea(int top_y = 0);
	void savePos();
	void restorePos();
public:
	
	row_index_t currentVisualRow() const;
	int currentVisualCol() const;
	
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
	
	AbstractCharacterBasedApplication();
	virtual ~AbstractCharacterBasedApplication();
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
	void showHeader(bool f);
	void showFooter(bool f);
	void setAutoLayout(bool f);
	void setDocument(const std::vector<Document::Line> *source);
	void setSelectionAnchor(SelectionAnchor::Enabled enabled, bool update_anchor, bool auto_scroll);
	void setNormalTextEditorMode(bool f);
	void setToggleSelectionAnchorEnabled(bool f);
	void setReadOnly(bool f);
	bool isReadOnly() const;
	void editPaste();
	void editCopy();
	void editCut();
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
	void logicalMoveToBottom2();
	void appendBulk(std::string_view const &str);
	void clear();
protected:
	std::vector<Document::Line> wrapLine(Document::Line line, std::mutex *mutex) const;
private:
	void _updateVisualLineByLogicalLine(col_index_t lrow, Document::Line const &ll, std::mutex *mutex);
	void _updateVisualLinesAll(bool force);
public:
	bool updateVisualLine(row_index_t lrow, bool force, std::mutex *mutex = nullptr);
	void updateVisualLinesAll()
	{
		invalidateVisualRowInfo(0);
	}
	void setWrappingMode(WrappingMode mode);
	AbstractCharacterBasedApplication::WrappingMode wrappingMode() const;

	void setCurrentLogicalRow(row_index_t row);
	void setCurrentLogicalCol(col_index_t col);
	row_index_t currentLogicalRow() const;
	col_index_t currentLogicalCol() const;
	bool isWidthFixed() const;
protected:
	void write_(char const *ptr, bool by_keyboard);
	void write_(QString const &text, bool by_keyboard);
	void makeColumnPosList(std::vector<int> *out);
	bool isValidRowIndex(row_index_t row_index) const;
	bool hasSelection() const;
	void updateSelectionAnchor1(bool auto_scroll);
	void updateSelectionAnchor2(bool auto_scroll);
};

class AbstractTextEditorApplication : public AbstractCharacterBasedApplication {
	
};

#endif // ABSTRACTCHARACTERBASEDAPPLICATION_H
