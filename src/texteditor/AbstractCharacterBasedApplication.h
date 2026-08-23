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

// class MyTextCodec {
// private:
// public:
// 	MyTextCodec() = default;
// 	MyTextCodec(char const *name) //@ TODO:
// 	{
// 	}
// 	QString toUnicode(QByteArray const &ba) const
// 	{
// 		return QString::fromUtf8(ba);
// 	}
// 	QByteArray fromUnicode(QString const &s) const
// 	{
// 		return s.toUtf8();
// 	}
// };

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
		varline_t text_ = std::string_view();
		struct Meta {
			LineType type = Normal;
			col_index_t logical_col = 0;
			int32_t line_number_override = -1;
			mutable std::shared_ptr<LineProperty> detail;
			mutable std::vector<Document::Line> visual_lines;
		} meta;
		
		Line() = default;
		
		static Line InvalidLine()
		{
			Line line;
			line.meta.type = Invalid;
			return line;
		}
		
		static Line NormalEmptyLine()
		{
			Line line;
			line.meta.type = Normal;
			return line;
		}
		
		explicit Line(std::vector<char> const &ba, LineType type = Normal)
			: text_(ba)
		{
			meta.type = type;
		}
		
		explicit Line(QByteArray const &ba, LineType type = Normal)
			: text_(std::vector<char>(ba.data(), ba.data() + ba.size()))
		{
			meta.type = type;
		}
		
		static Line None()
		{
			Line line;
			line.meta.type = Invalid;
			return line;
		}
		
		static Line View(std::string_view v, LineType type)
		{
			Line line;
			line.text_ = v;
			line.meta.type = type;
			return line;
		}
		
		static Line View(std::string_view v, Meta const &meta)
		{
			Line line;
			line.text_ = v;
			line.meta = meta;
			return line;
		}
		
		static Line View(std::string_view v)
		{
			return View(v, {});
		}
		
		static Line View(Line const &line)
		{
			if (std::holds_alternative<std::string_view>(line.text_)) {
				return line;
			}
			std::vector<char> const *v = std::get_if<std::vector<char>>(&line.text_);
			assert(v);
			return View(std::string_view(v->data(), v->size()), line.meta);
		}
		
		LineType type() const
		{
			return meta.type;
		}
		
		void set_line_number_override(int32_t num)
		{
			meta.line_number_override = num;
		}
		
		LineProperty *detail() const
		{
			return meta.detail.get();
		}
		
		LineProperty *newDetail()
		{
			meta.detail = std::make_shared<LineProperty>();
			return detail();
		}
		
		void clearDetail()
		{
			meta.detail.reset();
		}
		
		bool endsWithNewLine() const
		{
			int c = text().empty() ? 0 : text().back();
			return c == '\n' || c == '\r';
		}
		
		std::string_view text() const
		{
			if (std::holds_alternative<std::string_view>(text_)) {
				return std::get<std::string_view>(text_);
			}
			std::vector<char> const *v = std::get_if<std::vector<char>>(&text_);
			assert(v);
			return std::string_view(v->data(), v->size());
		}
		
		void set_text(const std::vector<char> &new_text)
		{
			text_ = new_text;
			meta.detail.reset();
			clear_visual_lines();
		}
		
		std::vector<char> *to_vector()
		{
			if (std::string_view *sv = std::get_if<std::string_view>(&text_)) {
				text_ = std::vector<char>(sv->data(), sv->data() + sv->size());
			}
			std::vector<char> *v = std::get_if<std::vector<char>>(&text_);
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
			meta.visual_lines.clear();
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
	int row = 0;
	int col = 0;
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
	row_index_t current_visual_row = 0;
	int current_visual_col = 0; // 桁位置
	int current_visual_col_hint = 0;
	int current_visual_pixel_x = 0; // 桁ピクセル座標
	int current_visual_pixel_y = 0; // 行ピクセル座標
	int saved_row = 0;
	int saved_col = 0;
	int saved_col_hint = 0;
	int current_char_span = 1;
	int scroll_row_pos = 0;
	int scroll_col_pos = 0;
	int viewport_org_x = 0;
	int viewport_org_y = 1;
	int viewport_width = 80;
	int viewport_height = 23;
	int tab_indent_size = 4;
	int bottom_line_y = -1;
	TextEditorEngine_sp engine;
	std::vector<Document::Line> visual_lines;
};

struct RowCol {
	int row = 0;
	int col = 0;
	RowCol(int row = 0, int col = 0)
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
	const int reference_char_width_ = 1; //@ TODO: remove
protected:

	std::vector<Document::Line> *_lines();
	
	std::vector<Document::Line> const *_lines() const
	{
		return const_cast<AbstractCharacterBasedApplication *>(this)->_lines();
	}
	
	row_index_t nlines() const;

	Document::Line *line(row_index_t row);
	
	Document::Line const *line(row_index_t row) const
	{
		return const_cast<AbstractCharacterBasedApplication *>(this)->line(row);
	}
	
	int char_screen_w() const;
	int char_screen_h() const;
	std::vector<Char16> *char_screen();
	std::vector<Char16> const *char_screen() const;
	std::vector<uint8_t> *line_flags();
	
	void initEditor();
protected:
	void fetchCurrentLine();
	void clearParsedLine();
	
	int currentVisualPixelX() const;
	void setCurrentVisualRow(row_index_t row);
	void setCurrentVisualCol(int col);
	
	int cursorCol() const;
	int cursorRow() const;
	
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

	void insertLine();
	void commitLine(const std::vector<Character> &vec);
	
	void doDelete();
	void doBackspace();
	
	bool isDialogMode();
	void setDialogMode(bool f);
	void closeDialog(bool result);
	void setDialogOption(QString const &title, QString const &value, const DialogHandler &handler);
	void execDialog(QString const &dialog_title, const QString &dialog_value, const DialogHandler &handler);
	
	virtual void invalidateVisualRowInfo(row_index_t vrow) {}
	virtual VisualRowInfo queryVisualRowInfo(row_index_t vrow) { return {}; }
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
	void setCursorCol_(int col, bool auto_scroll = true, bool by_mouse = false);
	std::vector<Document::Line> *documentLinesForWrite(bool check_readonly = true);
protected:
	void deselect();
	std::vector<Character> parseLogicalLine(const TextEditorContext *cx, row_index_t lrow) const;
	void parseCurrentLine(std::vector<Character> *chars, bool force);
	std::vector<Character> parseLine(const TextEditorContext *cx, const Document::Line *line) const;
	std::vector<Character> parseLine(int vrow);

	virtual void updateScrollBarRange() {}
	
	virtual void setCursorRow(int row, bool auto_scroll = true, bool by_mouse = false);
	virtual void setCursorCol(int col)
	{
		setCursorCol_(col, true, false);
	}
	void setCursorPosByMouse(RowCol pos, QPoint pt)
	{
		setCursorRow(pos.row, false, true);
		setCursorCol_(pos.col, false, true);
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
	
	void addNewLineToBottom();
	void appendNewLine(std::vector<Character> *vec);
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
	void moveCursorHome();
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
	bool isBottom() const;
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
	std::vector<Document::Line> doCharWrapLine(Document::Line line) const;
private:
	void updateVisualLines(row_index_t lrow, bool force);
public:
	void updateVisualLinesAll(bool force);
	void doWrapping();
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
