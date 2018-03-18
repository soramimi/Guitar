#ifndef ABSTRACTCHARACTERBASEDAPPLICATION_H
#define ABSTRACTCHARACTERBASEDAPPLICATION_H

#include <QList>
#include <QRect>
#include <QString>
#include <QByteArray>
#include <memory>
#include <vector>
#include <functional>
#include <QKeyEvent>

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

class Document {
public:
	struct Line {
		enum Type {
			Unknown,
			Normal,
			Add,
			Del,
		};
		int type = Unknown;
		int hunk_number = -1;
		int line_number = 0;
		QByteArray text;

		Line()
		{
		}

		explicit Line(QByteArray const &ba)
			: type(Normal)
			, text(ba)
		{
		}

		explicit Line(std::string const &str, Type type = Normal)
			: type(type)
		{
			text.append(str.c_str(), str.size());
		}
	};

	QList<Line> lines;
};

class TextEditorEngine {
public:
	Document document;
	TextEditorEngine()
	{
		document.lines.push_back(Document::Line(QByteArray()));
	}
};

struct SelectionAnchor {
	enum Enabled {
		No,
		EnabledHard,
		EnabledEasy,
	};

	Enabled enabled = No;
	int row = 0;
	int col = 0;
	bool operator == (SelectionAnchor const &a) const
	{
		return enabled == a.enabled && row == a.row && col == a.col;
	}
	bool operator != (SelectionAnchor const &a) const
	{
		return !operator == (a);
	}
};

typedef std::shared_ptr<TextEditorEngine> TextEditorEnginePtr;

struct TextEditorContext {
	QRect cursor_rect;
	bool single_line = false;
	int current_row = 0;
	int current_col = 0;
	int current_col_hint = 0;
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
	int tab_span = 4;
	int bottom_line_y = -1;
	TextEditorEnginePtr engine;
};

typedef std::function<void(bool, QString const &text)> DialogHandler;

class AbstractCharacterBasedApplication {
public:
	static const int LEFT_MARGIN = 6;
	static const int RIGHT_MARGIN = 10;

	enum class WriteMode {
		Insert,
		Overwrite,
	};

	enum class State {
		Normal,
		Exit,
	};

	struct CharAttr {
		uint16_t index;
		uint16_t flags = 0;
		CharAttr(int index = Normal)
			: index(index)
		{
		}
		bool operator == (CharAttr const &r) const
		{
			return index == r.index;
		}
		bool operator != (CharAttr const &r) const
		{
			return !operator == (r);
		}
		enum Index {
			Normal,
			Invert,
			Hilite,
		};
		enum Flag {
			Selected = 0x0001,
			CurrentLine = 0x0002,
		};
	};

	struct Option {
		CharAttr char_attr;
		QRect clip;
	};

	struct Character {
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
		int atts;
		FormattedLine(QString const &text, int atts)
			: text(text)
			, atts(atts)
		{
		}
		bool isSelected() const
		{
			return atts & 1;
		}
	};

	QList<FormattedLine> formatLine(const Document::Line &line, int tab_span, int anchor_a = -1, int anchor_b = -1) const;
private:
	struct Private;
	Private *m;
protected:
	SelectionAnchor selection_anchor_0;
	SelectionAnchor selection_anchor_1;
protected:

	std::vector<Character> *screen();
	std::vector<Character> const *screen() const;
	std::vector<uint8_t> *line_flags();

	void initEditor();

	void fetchCurrentLine();
	void clearParsedLine();

	int cursorX() const;
	int cursorY() const;

	int editorViewportWidth() const;
	int editorViewportHeight() const;

	virtual int print(int x, int y, QString const &text, Option const &opt);

	std::shared_ptr<TextEditorContext> editor_cx;
	std::shared_ptr<TextEditorContext> dialog_cx;

	TextEditorContext *cx();
	TextEditorContext const *cx() const;

	Document *document();
	int documentLines() const;

	bool isSingleLineMode() const;

	void ensureCurrentLineVisible();
	int decideColumnScrollPos() const;

	int calcVisualWidth(Document::Line const &line) const;

	int xScrollPos() const;
	int leftMargin() const;

	void makeBuffer();
	int printArea(const TextEditorContext *cx, SelectionAnchor const *sel_a = nullptr, SelectionAnchor const *sel_b = nullptr);

	int calcIndexToColumn(const std::vector<uint32_t> &vec, int index) const;

	virtual void updateVisibility(bool ensure_current_line_visible, bool change_col, bool auto_scroll) = 0;
	void commitLine(const std::vector<uint32_t> &vec);

	void doDelete();
	void doBackspace();

	bool isDialogMode();
	void setDialogMode(bool f);
	void closeDialog(bool result);
	void setDialogOption(const QString &title, QString value, DialogHandler handler);
	void execDialog(const QString &dialog_title, QString dialog_value, DialogHandler handler);
	void toggleSelectionAnchor();
private:
	int internalParseLine(std::vector<uint32_t> *vec, int increase_hint) const;
	void internalWrite(const ushort *begin, const ushort *end);
	void pressLetterWithControl(int c);
	void invalidateAreaBelowTheCurrentLine();
	void onQuit();
	void onOpenFile();
	void onSaveFile();
	void printInvertedBar(int x, int y, const char *text, int padchar);
	SelectionAnchor currentAnchor(SelectionAnchor::Enabled enabled);
	enum class EditOperation {
		Cut,
		Copy,
	};
	void editSelected(EditOperation op, std::vector<uint32_t> *cutbuffer);
	void deselect();
	int calcColumnToIndex(int column);
	void edit_(EditOperation op);
	bool isCurrentLineWritable() const;
	void initEngine(std::shared_ptr<TextEditorContext> cx);
	void writeCR();
	void fetchLine() const;
protected:

	void parseLine(std::vector<uint32_t> *vec, int increase_hint, bool force);
	QByteArray parsedLine() const;
	void setCursorRow(int row, bool shift = false, bool auto_scroll = true);
	void setCursorCol(int col, bool shift = false, bool auto_scroll = true);
	void setCursorPos(int row, int col, bool shift = false);
	void setCursorColByIndex(const std::vector<uint32_t> &vec, int col_index);
	int nextTabStop(int x) const;
	int scrollBottomLimit() const;
	bool isPaintingSuppressed() const;
	void setPaintingSuppressed(bool f);
	void scrollRight();
	void scrollLeft();

	void addNewLineToBottom();
	void appendNewLine(std::vector<uint32_t> *vec);
	void writeNewLine();
	void updateCursorPos(bool auto_scroll);

	QString statusLine() const;

	void preparePaintScreen();
	void setRecentlyUsedPath(const QString &path);
	QString recentlyUsedPath();
	void clearRect(int x, int y, int w, int h);
	void paintLineNumbers(std::function<void(int, QString, const Document::Line *)> draw);
	bool isAutoLayout() const;
	void invalidateArea(int top_y = 0);
	void savePos();
	void restorePos();
public:
	virtual void layoutEditor();
	void scrollUp();
	void scrollDown();
	void moveCursorOut();
	void moveCursorHome();
	void moveCursorEnd();
	void moveCursorUp();
	void moveCursorDown();
	void moveCursorLeft();
	void moveCursorRight();
	void movePageUp();
	void movePageDown();
	void scrollToTop();

	AbstractCharacterBasedApplication();
	virtual ~AbstractCharacterBasedApplication();
	TextEditorEnginePtr engine();
	int screenWidth() const;
	int screenHeight() const;
	void setScreenSize(int w, int h, bool update_layout);
	void setTextEditorEngine(TextEditorEnginePtr e);
	void openFile(const QString &path);
	void saveFile(const QString &path);
	void loadExampleFile();
	void pressEnter();
	void pressEscape();
	State state() const;
	bool isLineNumberVisible() const;
	void showLineNumber(bool show, int left_margin = LEFT_MARGIN);
	void showHeader(bool f);
	void showFooter(bool f);
	void setAutoLayout(bool f);
	void setDocument(const QList<Document::Line> *source);
	void setSelectionAnchor(SelectionAnchor::Enabled enabled, bool update_anchor, bool auto_scroll);
	bool isSelectionAnchorEnabled() const;
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
	void moveToBottom();
	bool isBottom() const;
	void setLineMargin(int n);
	void write(uint32_t c, bool by_keyboard);
	void write(const char *ptr, int len, bool by_keyboard);
	void write(QKeyEvent *e);
	void setTextCodec(QTextCodec *codec);
	void setCursorVisible(bool show);
	bool isCursorVisible();
protected:
	void write_(const char *ptr, bool by_keyboard);
	void write_(QString text, bool by_keyboard);
};

class AbstractTextEditorApplication : public AbstractCharacterBasedApplication {

};


#endif // ABSTRACTCHARACTERBASEDAPPLICATION_H
