
#include "AbstractCharacterBasedApplication.h"
#include "UnicodeWidth.h"
#include "unicode.h"
#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QFile>
#include <QTextCodec>
#include <memory>

using WriteMode = AbstractCharacterBasedApplication::WriteMode;
using FormattedLine = AbstractCharacterBasedApplication::FormattedLine2;

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
	int header_line = 0;
	int footer_line = 0;
	int screen_width = 80;
	int screen_height = 24;
	bool auto_layout = false;
	QString recently_used_path;
	bool show_line_number = true;
	int left_margin = AbstractCharacterBasedApplication::LEFT_MARGIN;
	QString dialog_title;
	QString dialog_value;
	std::vector<Character> screen;
	std::vector<uint8_t> line_flags;
	int parsed_row_index = -1; //@
	int parsed_col_index = -1; //@
	bool parsed_for_edit = false;
	QByteArray current_line_data;
	std::vector<AbstractCharacterBasedApplication::Char> parsed_current_line;
	QList<Document::CharAttr_> syntax_table;
	bool dialog_mode = false;
	DialogHandler dialog_handler;
	bool is_painting_suppressed = false;
	int valid_line_index = -1;
	int line_margin = 3;
	WriteMode write_mode = WriteMode::Insert;
	QTextCodec *text_codec = nullptr;
	Qt::KeyboardModifiers keyboard_modifiers = Qt::KeyboardModifier::NoModifier;
	bool ctrl_modifier = false;
	bool shift_modifier = false;
	EsccapeSequence escape_sequence;

	bool cursor_moved_by_mouse = false;
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

void AbstractCharacterBasedApplication::setTextCodec(QTextCodec *codec)
{
	m->text_codec = codec;
	clearParsedLine();
	invalidateArea();
	makeBuffer();
}

void AbstractCharacterBasedApplication::setAutoLayout(bool f)
{
	m->auto_layout = f;
	layoutEditor();
}

void AbstractCharacterBasedApplication::showHeader(bool f)
{
	m->header_line = f ? 1 : 0;
	layoutEditor();
}

void AbstractCharacterBasedApplication::showFooter(bool f)
{
	m->footer_line = f ? 1 : 0;
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

void AbstractCharacterBasedApplication::retrieveLastText(std::vector<char> *out, int maxlen) const
{
	const_cast<AbstractCharacterBasedApplication *>(this)->document()->retrieveLastText(out, maxlen);
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

std::vector<AbstractCharacterBasedApplication::Character> *AbstractCharacterBasedApplication::char_screen()
{
	return &m->screen;
}

int AbstractCharacterBasedApplication::char_screen_w() const
{
	return m->screen_width;
}

int AbstractCharacterBasedApplication::char_screen_h() const
{
	return m->screen_height;
}

const std::vector<AbstractCharacterBasedApplication::Character> *AbstractCharacterBasedApplication::char_screen() const
{
	return &m->screen;
}

std::vector<uint8_t> *AbstractCharacterBasedApplication::line_flags()
{
	return &m->line_flags;
}

void AbstractCharacterBasedApplication::makeBuffer()
{
	int w = screenWidth();
	int h = screenHeight();
	int size = w * h;
	m->screen.resize(size);
	std::fill(m->screen.begin(), m->screen.end(), Character());
	m->line_flags.resize(h);
}

void AbstractCharacterBasedApplication::layoutEditor()
{
	makeBuffer();
	editor_cx->viewport_org_x = leftMargin_();
	editor_cx->viewport_org_y = m->header_line;
	editor_cx->viewport_width = screenWidth() - cx()->viewport_org_x;
	editor_cx->viewport_height = screenHeight() - (m->header_line + m->footer_line);
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

QList<FormattedLine> AbstractCharacterBasedApplication::formatLine_(Document::Line const &line, int tab_span, int anchor_a, int anchor_b) const
{
	QByteArray ba;
	if (m->text_codec) {
		ba = m->text_codec->toUnicode(line.text).toUtf8();
	} else {
		ba = line.text;
	}

	std::vector<char16_t> vec;
    vec.reserve((size_t)ba.size() + 100);
    size_t len = (size_t)ba.size();
	QList<FormattedLine2> res;

	int col = 0;
	int col_start = col;

	bool flag_a = false;
	bool flag_b = false;

	auto Color = [&](size_t offset, size_t *next_offset){
		int i = findSyntax(&m->syntax_table, offset);
		if (i < m->syntax_table.size() && m->syntax_table[i].offset <= offset) {
			if (next_offset) {
				if (i + 1 < m->syntax_table.size()) {
					*next_offset = m->syntax_table[i + 1].offset;
				} else {
                    *next_offset = (size_t)-1;
				}
			}
			static int color[] = {
				0x000000,
				0x0000ff,
				0x00ff00,
				0x00ffff,
				0xff0000,
				0xff00ff,
				0xffff00,
				0xffffff,
			};
			i = m->syntax_table[i].color;
			if (i >= 0 && i < 8) {
				return color[i];
			}
		}
		if (next_offset) {
            *next_offset = (size_t)-1;
		}
		return 0;
	};

	auto Flush = [&](size_t offset, size_t *next_offset){
		if (!vec.empty()) {
			int atts = 0;
			atts |= Color(offset, next_offset);
			if (anchor_a >= 0 || anchor_b >= 0) {
				if ((anchor_a < 0 || col_start >= anchor_a) && (anchor_b == -1 || col_start < anchor_b)) {
					atts |= FormattedLine2::Selected;
				}
			}
			char16_t const *left = &vec[0];
			char16_t const *right = left + vec.size();
			while (left < right && (right[-1] == '\r' || right[-1] == '\n')) right--;
			if (left < right) {
				res.push_back(FormattedLine2(QString::fromUtf16(left, int(right - left)), atts));
			}
			vec.clear();
		} else {
            *next_offset = (size_t)-1;
		}
		col_start = col;
	};

    size_t offset = 0;
    size_t next_offset = (size_t)-1;
	if (len > 0) {
		{
			size_t o = line.byte_offset;
			int i = findSyntax(&m->syntax_table, o);
			if (i < m->syntax_table.size() && m->syntax_table[i].offset <= o) {
				if (i + 1 < m->syntax_table.size()) {
					o = m->syntax_table[i + 1].offset;
					if (o != (size_t)-1) {
						next_offset = o;
					}
				}
			}
		}
		utf8 u8(ba.data(), len);
		u8.to_utf32([&](uint32_t c){
			if (line.byte_offset + u8.offset() == next_offset) {
				Flush(line.byte_offset + offset, &next_offset);
				offset += u8.offset();
			}
			if (c == '\t') {
				do {
					vec.push_back(' ');
					col++;
				} while (col % tab_span != 0);
			} else if (c < ' ') {
				// nop
			} else {
				int cw = charWidth(c);
				if (c < 0xffff) {
                    vec.push_back((ushort)c);
				} else {
                    unsigned int a = c >> 16;
					if (a > 0 && a <= 0x20) {
						a--;
                        unsigned int b = c & 0x03ff;
						a = (a << 6) | ((c >> 10) & 0x003f);
						a |= 0xd800;
                        b |= 0xdc00;
                        vec.push_back((ushort)a);
                        vec.push_back((ushort)b);
					}
				}
				col += cw;
			}
			if ((anchor_a >= 0 || anchor_b >= 0) && anchor_a != anchor_b) {
				if (!flag_a && col >= anchor_a) {
					Flush(line.byte_offset + offset, &next_offset);
					flag_a = true;
				}
				if (!flag_b && col >= anchor_b) {
					Flush(line.byte_offset + offset, &next_offset);
					flag_b = true;
				}
			}
			return true;
		});
	}
	Flush(line.byte_offset + offset, &next_offset);
	return res;
}

bool AbstractCharacterBasedApplication::isValidRowIndex(int row_index) const
{
	return row_index >= 0 && row_index < engine()->document.lines.size();
}

QList<AbstractCharacterBasedApplication::FormattedLine2> AbstractCharacterBasedApplication::formatLine2_(int row_index) const
{
	QList<FormattedLine2> formatted_lines;
	if (isValidRowIndex(row_index)) {
		Document::Line const *line = &engine()->document.lines[row_index];
		formatted_lines = formatLine_(*line, cx()->tab_span);
	}
	return formatted_lines;
}

/**
 * @brief 現在行を取得
 * @param row
 * @return
 */
QByteArray AbstractCharacterBasedApplication::fetchLine(int row) const
{
	QByteArray line;
	int lines = documentLines();
	if (row >= 0 && row < lines) {
		line = cx()->engine->document.lines[row].text;
	}
	return line;
}

int AbstractCharacterBasedApplication::currentRow() const
{
	return cx()->current_row;
}

int AbstractCharacterBasedApplication::currentCol() const
{
	return cx()->current_col;
}

int AbstractCharacterBasedApplication::currentColX() const
{
	return cx()->current_col_pixel_x;
}

void AbstractCharacterBasedApplication::setCurrentRow(int row)
{
	cx()->current_row = row;
}

void AbstractCharacterBasedApplication::setCurrentCol(int col)
{
	cx()->current_col = col;
}

void AbstractCharacterBasedApplication::fetchCurrentLine() const
{
	int row = currentRow();
	m->current_line_data = fetchLine(row);
	m->parsed_row_index = row;
}

void AbstractCharacterBasedApplication::clearParsedLine()
{
	m->parsed_row_index = -1;
	m->parsed_for_edit = false;
	m->current_line_data.clear();
}

int AbstractCharacterBasedApplication::cursorCol() const
{
	return currentCol() - cx()->scroll_col_pos;
}

int AbstractCharacterBasedApplication::cursorRow() const
{
	return currentRow() - cx()->scroll_row_pos;
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

bool AbstractCharacterBasedApplication::isPaintingSuppressed() const
{
	return m->is_painting_suppressed;
}

void AbstractCharacterBasedApplication::setPaintingSuppressed(bool f)
{
	m->is_painting_suppressed = f;
}

void AbstractCharacterBasedApplication::commitLine(std::vector<Char> const &vec)
{
	if (isReadOnly()) return;

	QByteArray ba;
	if (!vec.empty()){
		std::vector<uint32_t> v;
		v.reserve(vec.size());
		for (Char const &c : vec) {
			v.push_back(c.unicode);
		}
		utf32 u32(&v[0], v.size());
		u32.to_utf8([&](char c, int pos){
			(void)pos;
			ba.push_back(c);
			return true;
		});
	}
	if (m->parsed_row_index == 0 && engine()->document.lines.empty()) {
		Document::Line newline;
		newline.type = Document::Line::Normal;
		engine()->document.lines.push_back(newline);
	}
	Document::Line *line = &engine()->document.lines[m->parsed_row_index];
	if (m->parsed_row_index == 0) {
		line->byte_offset = 0;
		line->line_number = (line->type == Document::Line::Unknown) ? 0 : 1;
	}
	line->text = ba;

	if (m->valid_line_index > m->parsed_row_index) {
		m->valid_line_index = m->parsed_row_index;
	}

	int y = m->parsed_row_index - cx()->scroll_row_pos + cx()->viewport_org_y;
	if (y >= 0 && y < (int)m->line_flags.size()) {
		m->line_flags[y] |= LineChanged;
	}
}

/**
 * @brief 行のレイアウトを解析
 * @param vec
 * @param increase_hint
 * @param force
 */
std::vector<AbstractCharacterBasedApplication::Char> *AbstractCharacterBasedApplication::parseCurrentLine(std::vector<Char> *vec, int increase_hint, bool force)
{
	if (force) {
		clearParsedLine();
	}

	if (!vec) {
		vec = &m->parsed_current_line;
	}

	if (force || !m->parsed_for_edit) {
		fetchCurrentLine();
		m->parsed_col_index = internalParseLine(m->current_line_data, currentCol(), vec, increase_hint);
		m->parsed_for_edit = true;
	} else {
		if (vec) {
			*vec = m->parsed_current_line;
		}
	}

	return vec;
}

/**
 * @brief 桁位置を求める
 * @param parsed_line
 * @param current_col
 * @param vec
 * @param increase_hint
 * @return
 */
int AbstractCharacterBasedApplication::internalParseLine(QByteArray const &parsed_line, int current_col, std::vector<Char> *vec, int increase_hint) const
{
	vec->clear();

	int index = -1;
	int col = 0;
	int len = parsed_line.size();
	if (len > 0) {
		vec->reserve(len + increase_hint);
		char const *src = parsed_line.data();
		utf8 u8(src, len);
		while (1) {
			int n = 0;
			uint32_t c = u8.next();
			if (c == 0) {
				n = 1;
			} else {
				if (c == '\t') {
					int z = nextTabStop(col);
					n = z - col;
				} else {
					n = charWidth(c);
				}
			}
			if (col <= current_col && col + n > current_col) {
				index = (int)vec->size();
			}
			if (c == 0) break;
			col += n;
			vec->emplace_back(c);
		}
	}
	return index;
}

/**
 * @brief 行の桁位置を求める
 * @param row
 * @param vec
 */
void AbstractCharacterBasedApplication::parseLine(int row, std::vector<Char> *vec) const
{
	QByteArray line = fetchLine(row);
	internalParseLine(line, -1, vec, 0);
}

bool AbstractCharacterBasedApplication::isCurrentLineWritable() const
{
	if (isReadOnly()) return false;

	int row = currentRow();
	if (row >= 0 && row < cx()->engine->document.lines.size()) {
		if (cx()->engine->document.lines[row].type != Document::Line::Unknown) {
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
	if (opt.char_attr.flags & CharAttr::Selected) {
		attr.index = CharAttr::Invert;
	}
	if (opt.char_attr.flags & CharAttr::CurrentLine) {
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
		if (changed) {
			m->line_flags[y] |= LineChanged;
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

TextEditorEnginePtr AbstractCharacterBasedApplication::engine() const
{
	Q_ASSERT(cx()->engine);
	return cx()->engine;
}

void AbstractCharacterBasedApplication::setTextEditorEngine(TextEditorEnginePtr const &e)
{
	cx()->engine = e;
}

void AbstractCharacterBasedApplication::clear()
{
	setDocument(nullptr);
}

void AbstractCharacterBasedApplication::setDocument(QList<Document::Line> const *source)
{
	if (source) {
		document()->lines = *source;
	} else {
		document()->lines.clear();
	}
}

void AbstractCharacterBasedApplication::openFile(QString const &path)
{
	document()->lines.clear();
	QFile file(path);
	if (file.open(QFile::ReadOnly)) {
		size_t offset = 0;
		QString line;
		unsigned int linenum = 0;
		while (!file.atEnd()) {
			linenum++;
			Document::Line line(file.readLine());
			line.byte_offset = offset;
            line.type = Document::Line::Normal;
            line.line_number = (int)linenum;
			document()->lines.push_back(line);
			offset += line.text.size();
		}
		int n = line.size();
		if (n > 0) {
			ushort const *p = line.utf16();
			ushort c = p[n - 1];
			if (c == '\r' || c == '\n') {
				document()->lines.push_back(Document::Line(QByteArray()));
			}
		}
		m->valid_line_index = (int)document()->lines.size();
		setRecentlyUsedPath(path);
	}

	if (document()->lines.empty()) {
		Document::Line line;
		line.type = Document::Line::Normal;
		line.line_number = 1;
		document()->lines.push_back(line);
	}

	scrollToTop();
}

void AbstractCharacterBasedApplication::saveFile(QString const &path)
{
	QFile file(path);
	if (file.open(QFile::WriteOnly)) {
		for (Document::Line const &line : document()->lines) {
			file.write(line.text);
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

int AbstractCharacterBasedApplication::documentLines() const
{
	return const_cast<AbstractCharacterBasedApplication *>(this)->document()->lines.size();
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
	int pos = cx()->scroll_row_pos;
	int top = currentRow() - margin;
	int bottom = currentRow() + 1 - editorViewportHeight() + margin;
	if (pos > top)    pos = top;
	if (pos < bottom) pos = bottom;
	if (pos < 0) pos = 0;
	if (cx()->scroll_row_pos != pos) {
		cx()->scroll_row_pos = pos;
		invalidateArea();
	}
}

int AbstractCharacterBasedApplication::decideColumnScrollPos() const
{//@
	int x = currentCol();
	int w = editorViewportWidth() - RIGHT_MARGIN;
	if (w < 0) w = 0;
	return x > w ? (currentCol() - w) : 0;
}

int AbstractCharacterBasedApplication::calcVisualWidth(const Document::Line &line) const
{
	QList<FormattedLine2> lines = formatLine_(line, cx()->tab_span);
	int x = 0;
	for (FormattedLine2 const &line : lines) {
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
			if (c == '\t') {
//				x += cx()->tab_span;
//				x -= x % cx()->tab_span;
				x++;
			} else {
//				x += charWidth(c);
				x++;
			}
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
			m->screen[o] = Character();
		}
	}
}

void AbstractCharacterBasedApplication::savePos()
{
	TextEditorContext *p = editor_cx.get();
	if (p) {
		p->saved_row = p->current_row;
		p->saved_col = p->current_col;
		p->saved_col_hint = p->current_col_hint;
	}
}

void AbstractCharacterBasedApplication::restorePos()
{
	TextEditorContext *p = editor_cx.get();
	if (p) {
		p->current_row = p->saved_row;
		p->current_col = p->saved_col;
		p->current_col_hint = p->saved_col_hint;
	}
}

void AbstractCharacterBasedApplication::deselect()
{
	selection_end = {};
	selection_start = {};
}

bool AbstractCharacterBasedApplication::hasSelection() const
{
	return selection_end.enabled == SelectionAnchor::False;
}

void AbstractCharacterBasedApplication::updateSelectionAnchor1(bool auto_scroll)
{
	if (isShiftModifierPressed()) {
		if (selection_end.enabled == SelectionAnchor::False) {
			setSelectionAnchor(SelectionAnchor::True, true, auto_scroll);
			selection_start = selection_end;
		}
	} else if (selection_end.enabled == SelectionAnchor::True) {
		// 選択中でShiftが押されていなければ選択解除
		setSelectionAnchor(SelectionAnchor::False, false, auto_scroll);
	}
}

void AbstractCharacterBasedApplication::updateSelectionAnchor2(bool auto_scroll)
{
	if (selection_end.enabled == SelectionAnchor::True) {
		// 選択中なら、現在位置で更新
		setSelectionAnchor(selection_end.enabled, true, auto_scroll);
	}
}

void AbstractCharacterBasedApplication::setCursorRow(int row, bool auto_scroll, bool by_mouse)
{
	if (currentRow() == row) return;

	updateSelectionAnchor1(false);

	setCurrentRow(row);

	updateSelectionAnchor2(auto_scroll);

	m->cursor_moved_by_mouse = by_mouse;
}

void AbstractCharacterBasedApplication::setCursorCol_(int col, bool auto_scroll, bool by_mouse)
{
	if (currentCol() == col) {
		cx()->current_col_hint = col;
		return;
	}

	updateSelectionAnchor1(false);

	setCurrentCol(col);
	cx()->current_col_hint = col;

	updateSelectionAnchor2(auto_scroll);

	m->cursor_moved_by_mouse = by_mouse;
}

int AbstractCharacterBasedApplication::nextTabStop(int x) const
{
	x += cx()->tab_span;
	x -= x % cx()->tab_span;
	return x;
}

void AbstractCharacterBasedApplication::editSelected(EditOperation op, std::vector<Char> *cutbuffer)
{
	if (isReadOnly() && op == EditOperation::Cut) {
		op = EditOperation::Copy;
	}

	SelectionAnchor a = selection_end;
	SelectionAnchor b = selection_start;
	if (!a.enabled) return;
	if (!b.enabled) return;
	if (a == b) return;

	auto UpdateVisibility = [&](){
		updateVisibility(false, false, false);
	};

	if (cutbuffer) {
		cutbuffer->clear();
	}
	std::list<std::vector<Char>> cutlist;

	if (a.row > b.row) {
		std::swap(a, b);
	} else if (a.row == b.row) {
		if (a.col > b.col) {
			std::swap(a, b);
		}
	}

	int curr_row = currentRow();
	int curr_col = currentCol();

	setCurrentRow(b.row);
	setCurrentCol(b.col);

	if (a.row == b.row) {
		std::vector<Char> vec1;
		parseCurrentLine(&vec1, 0, true);
		auto begin = vec1.begin() + calcColumnToIndex(a.col);
		auto end = vec1.begin() + calcColumnToIndex(b.col);
		if (cutbuffer) {
			std::vector<Char> cut;
			cut.insert(cut.end(), begin, end);
			cutlist.push_back(std::move(cut));
		}
		if (op == EditOperation::Cut) {
			vec1.erase(begin, end);
			commitLine(vec1);
			UpdateVisibility();
		}
	} else {
		std::vector<Char> vec1;
		parseCurrentLine(&vec1, 0, true);
		{
			auto begin = vec1.begin();
			auto end = vec1.begin() + calcColumnToIndex(b.col);
			if (cutbuffer) {
				std::vector<Char> cut;
				cut.insert(cut.end(), begin, end);
				cutlist.push_back(std::move(cut));
			}
			if (op == EditOperation::Cut) {
				vec1.erase(begin, end);
				commitLine(vec1);
				UpdateVisibility();
			}
		}
		int n = b.row - a.row;
		for (int i = 0; i < n; i++) {
			QList<Document::Line> *lines = &cx()->engine->document.lines;
			if (cutbuffer && i > 0) {
				setCurrentRow(b.row - i);
				setCurrentCol(0);
				std::vector<Char> cut;
				parseCurrentLine(&cut, 0, true);
				cutlist.push_back(std::move(cut));
			}
			if (op == EditOperation::Cut) {
				lines->erase(lines->begin() + b.row - i);
			}
		}

		setCurrentRow(a.row);
		setCurrentCol(a.col);
		int index = calcColumnToIndex(a.col);
		std::vector<Char> vec2;
		parseCurrentLine(&vec2, 0, true);
		if (cutbuffer) {
			std::vector<Char> cut;
			cut.insert(cut.end(), vec2.begin() + index, vec2.end());
			cutlist.push_back(std::move(cut));
		}

		if (op == EditOperation::Cut) {
			vec2.resize(index);
			vec2.insert(vec2.end(), vec1.begin(), vec1.end());
			commitLine(vec2);
			UpdateVisibility();
		}
	}

	if (cutbuffer) {
		size_t size = 0;
		for (std::vector<Char> const &v : cutlist) {
			size += v.size();
		}
		cutbuffer->reserve(size);
		for (auto it = cutlist.rbegin(); it != cutlist.rend(); it++) {
			std::vector<Char> const &v = *it;
			cutbuffer->insert(cutbuffer->end(), v.begin(), v.end());
		}
	}

	if (op == EditOperation::Cut) {
		deselect();
		setCursorPos(a.row, a.col);
		invalidateArea(a.row - cx()->scroll_row_pos);
	} else {
		setCurrentRow(curr_row);
		setCurrentCol(curr_col);
		invalidateArea(curr_row - cx()->scroll_row_pos);
	}

	clearParsedLine();
	UpdateVisibility();
}

void AbstractCharacterBasedApplication::edit_(EditOperation op)
{
	std::vector<Char> cutbuf;
	editSelected(op, &cutbuf);
	if (cutbuf.empty()) return;

	std::vector<uint32_t> u32buf;
	u32buf.reserve(cutbuf.size());
	for (Char const &c : cutbuf) {
		u32buf.push_back(c.unicode);
	}

	std::vector<char16_t> u16buf;
	u16buf.reserve(1024);
	utf32(u32buf.data(), u32buf.size()).to_utf16([&](uint16_t c){
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
	if (selection_end.enabled && selection_start.enabled) {
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

	parseCurrentLine(nullptr, 0, false);
	std::vector<Char> *vec = &m->parsed_current_line;
	int index = m->parsed_col_index;
	int c = -1;
	if (index >= 0 && index < (int)vec->size()) {
		c = (*vec)[index].unicode;
	}
	if (c == '\n' || c == '\r' || c == -1) {
		if (index == 0) {
			m->parsed_row_index--;
		}
		invalidateAreaBelowTheCurrentLine();
		if (isSingleLineMode()) {
			// nop
		} else {
			if (c != -1) {
				vec->erase(vec->begin() + index);
				if (c == '\r' && index < (int)vec->size() && (*vec)[index].unicode == '\n') {
					vec->erase(vec->begin() + index);
				}
			}
			if (vec->empty()) {
				clearParsedLine();
				if (currentRow() + 1 < (int)cx()->engine->document.lines.size()) {
					cx()->engine->document.lines.erase(cx()->engine->document.lines.begin() + currentRow());
				}
			} else {
				commitLine(*vec);
				setCursorCol(index);
				if (index == (int)vec->size()) {
					int nextrow = currentRow() + 1;
					int lines = documentLines();
					if (nextrow < lines) {
						Document::Line *ba1 = &cx()->engine->document.lines[currentRow()];
						Document::Line const &ba2 = cx()->engine->document.lines[nextrow];
						ba1->text.append(ba2.text);
						cx()->engine->document.lines.erase(cx()->engine->document.lines.begin() + nextrow);
					}
				}
			}
			clearParsedLine();
			updateVisibility(true, true, true);
		}
	} else {
		vec->erase(vec->begin() + index);
		commitLine(*vec);
		setCursorCol(index);
		updateVisibility(true, true, true);
	}
}

void AbstractCharacterBasedApplication::doBackspace()
{
	if (isReadOnly()) return;
	if (isTerminalMode()) return;

	if (deleteIfSelected()) {
		return ;
	}

	if (currentRow() > 0 || currentCol() > 0) {
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
		dialog_cx->engine->document.lines.push_back(Document::Line(m->dialog_value.toUtf8()));
		editor_cx->viewport_height = screenHeight() - m->header_line - 2;
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

void AbstractCharacterBasedApplication::closeDialog(bool result)
{
	if (isDialogMode()) {
		deselect();
		QString line;
		if (!dialog_cx->engine->document.lines.empty()) {
			Document::Line const &l = dialog_cx->engine->document.lines.front();
			line = QString::fromUtf8(l.text);
		}
		setDialogMode(false);
		if (m->dialog_handler) {
			m->dialog_handler(result, line);
		}
		return;
	}
}

int AbstractCharacterBasedApplication::calcColumnToIndex(int column)
{
	int index = 0;
	if (column > 0) {
		fetchCurrentLine();
		int col = 0;
		int len = m->current_line_data.size();
		if (len > 0) {
			char const *src = m->current_line_data.data();
			utf8 u8(src, len);
			while (1) {
				uint32_t c = u8.next();
				int n = 0;
				if (c == '\r' || c == '\n' || c == 0) {
					break;
				}
				if (c == '\t') {
					int z = nextTabStop(col);
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
	return index;
}

void AbstractCharacterBasedApplication::invalidateArea(int top_y)
{
	int y0 = cx()->viewport_org_y;
	int y1 = cx()->viewport_height + y0;
	top_y += y0;
	if (y0 < top_y) y0 = top_y;
	int n = (int)m->line_flags.size();
	if (y0 < 0) y0 = 0;
	if (y1 > n) y1 = n;
	for (int y = y0; y < y1; y++) {
		m->line_flags[y] |= LineChanged;
	}
}

void AbstractCharacterBasedApplication::invalidateAreaBelowTheCurrentLine()
{
	int y;

	y = m->parsed_row_index;
	if (m->valid_line_index > y) {
		m->valid_line_index = y;
	}

	y = m->parsed_row_index - cx()->scroll_row_pos;
	invalidateArea(y);
}

int AbstractCharacterBasedApplication::scrollBottomLimit() const
{
	return documentLines() - editorViewportHeight() / 2;
}

int AbstractCharacterBasedApplication::scrollBottomLimit2() const
{
	return documentLines() - editorViewportHeight();
}

void AbstractCharacterBasedApplication::writeCR()
{
	deleteIfSelected();

	setCursorCol(0);
	clearParsedLine();
	updateVisibility(true, true, true);
}

void AbstractCharacterBasedApplication::moveCursorOut()
{
	setCursorRow(-1);
}

void AbstractCharacterBasedApplication::moveCursorHome()
{
	fetchCurrentLine();
	QString line = m->current_line_data;
	ushort const *ptr = line.utf16();
	ushort const *end = ptr + line.size();
	int x = 0;
	while (1) {
		int c = -1;
		if (ptr < end) {
			c = *ptr;
			ptr++;
		}
		if (c == ' ') {
			x++;
		} else if (c == '\t') {
			x = nextTabStop(x);
		} else {
			break;
		}
	}
	if (x == currentCol()) {
		x = 0;
	}
	setCursorCol(x);
	clearParsedLine();
	updateVisibility(true, true, true);
}

void AbstractCharacterBasedApplication::moveCursorEnd()
{
	fetchCurrentLine();
	QByteArray line = m->current_line_data;
	int col = calcVisualWidth(Document::Line(line));
	setCursorCol(col);
	clearParsedLine();
	updateVisibility(true, true, true);
}

void AbstractCharacterBasedApplication::scrollUp()
{
	if (cx()->scroll_row_pos > 0) {
		cx()->scroll_row_pos--;
		invalidateArea();
		clearParsedLine();
		updateVisibility(false, false, true);
	}
}

void AbstractCharacterBasedApplication::scrollDown()
{
	int limit = scrollBottomLimit();
	if (cx()->scroll_row_pos < limit) {
		cx()->scroll_row_pos++;
		invalidateArea();
		clearParsedLine();
		updateVisibility(false, false, true);
	}
}

void AbstractCharacterBasedApplication::moveCursorUp()
{
	if (isSingleLineMode()) {
		// nop
	} else if (currentRow() > 0) {
		setCursorRow(currentRow() - 1); // カーソルを1行上へ
		clearParsedLine();
		updateVisibility(true, false, true);
	}
}

void AbstractCharacterBasedApplication::moveCursorDown()
{
	if (isSingleLineMode()) {
		// nop
	} else if (currentRow() + 1 < (int)document()->lines.size()) {
		setCursorRow(currentRow() + 1); // カーソルを1行下へ
		clearParsedLine();
		updateVisibility(true, false, true);
	}
}

void AbstractCharacterBasedApplication::scrollToTop()
{
	if (isSingleLineMode()) return;

	setCursorRow(0);
	setCursorCol(0);
	cx()->scroll_row_pos = 0;
	invalidateArea();
	clearParsedLine();
	updateVisibility(true, false, true);
}

void AbstractCharacterBasedApplication::moveCursorLeft()
{
	if (!isShiftModifierPressed() && selection_end.enabled && selection_start.enabled) { // 選択領域があったら
		if (selection_end != selection_start) {
			SelectionAnchor a = std::min(selection_end, selection_start);
			deselect();
			setCursorRow(a.row);
			setCursorCol(a.col);
			updateVisibility(true, true, true);
			return;
		}
	}

	if (currentCol() == 0) { // 行頭なら
		if (isSingleLineMode()) {
			// nop
		} else {
			if (currentRow() > 0) {
				setCursorRow(currentRow() - 1); // 上へ移動
				clearParsedLine();
				moveCursorEnd(); // 行末へ移動
			}
		}
		return;
	}

	setCursorCol(currentCol() - 1);
	updateVisibility(true, true, true);
}

void AbstractCharacterBasedApplication::moveCursorRight()
{
	if (!isShiftModifierPressed() && selection_end.enabled && selection_start.enabled) { // 選択領域があったら
		if (selection_end != selection_start) {
			SelectionAnchor a = std::max(selection_end, selection_start);
			deselect();
			setCursorRow(a.row);
			setCursorCol(a.col);
			updateVisibility(true, true, true);
			return;
		}
	}

	int col = 0;
	int i = 0;
	while (1) {
		int c = -1;
		if (i < (int)m->parsed_current_line.size()) {
			c = m->parsed_current_line[i].unicode;
		}
		if (c == '\r' || c == '\n' || c == -1) {
			if (!isSingleLineMode()) {
				int nextrow = currentRow() + 1;
				int lines = document()->lines.size();
				if (nextrow < lines) {
					setCursorPos(nextrow, 0);
					clearParsedLine();
					updateVisibility(true, true, true);
					return;
				}
			}
			break;
		}
		if (c == '\t') { //@
//			col = nextTabStop(col);
			col++;
		} else {
//			col += charWidth(c);
			col++;
		}
		if (col > currentCol()) {
			break;
		}
		i++;
	}
	if (col != currentCol()) {
		setCursorCol(col);
		clearParsedLine();
		updateVisibility(true, true, true);
	}
}

void AbstractCharacterBasedApplication::movePageUp()
{
	if (!isSingleLineMode()) {
		int step = editorViewportHeight();
		setCursorRow(currentRow() - step);
		cx()->scroll_row_pos -= step;
		if (currentRow() < 0) {
			setCurrentRow(0);
		}
		if (cx()->scroll_row_pos < 0) {
			cx()->scroll_row_pos = 0;
		}
		invalidateArea();
		clearParsedLine();
		updateVisibility(true, false, true);
	}
}

void AbstractCharacterBasedApplication::movePageDown()
{
	if (!isSingleLineMode()) {
		int limit = documentLines();
		if (limit > 0) {
			limit--;
			int step = editorViewportHeight();
			setCursorRow(currentRow() + step);
			cx()->scroll_row_pos += step;
			if (currentRow() > limit) {
				setCurrentRow(limit);
			}
			limit = scrollBottomLimit();
			if (cx()->scroll_row_pos > limit) {
				cx()->scroll_row_pos = limit;
			}
		} else {
			setCursorRow(0);
			cx()->scroll_row_pos = 0;
		}
		invalidateArea();
		clearParsedLine();
		updateVisibility(true, false, true);
	}
}

void AbstractCharacterBasedApplication::addNewLineToBottom()
{
	int row = cx()->engine->document.lines.size();
	if (currentRow() >= row) {
		setCursorPos(row, 0);
		cx()->engine->document.lines.push_back(Document::Line(QByteArray()));
	}
}

void AbstractCharacterBasedApplication::appendNewLine(std::vector<Char> *vec)
{
	if (isSingleLineMode()) return;

//	vec->emplace_back('\r');
	vec->emplace_back('\n');
}

void AbstractCharacterBasedApplication::writeNewLine()
{
	if (isReadOnly()) return;
	if (isSingleLineMode()) return;

	invalidateAreaBelowTheCurrentLine();

	std::vector<Char> curr_line;
	parseCurrentLine(&curr_line, 0, false);
	int index = m->parsed_col_index;
	if (index < 0) {
		addNewLineToBottom();
		index = 0;
	}
	std::vector<Char> next_line;
	// split line
	next_line.insert(next_line.end(), curr_line.begin() + index, curr_line.end());
	// shrink current line
	curr_line.resize(index);
	// append new line code
	appendNewLine(&curr_line);
	// next line index
	setCurrentRow(m->parsed_row_index + 1);
	// commit current line
	commitLine(curr_line);
	// insert next line
	m->parsed_row_index = currentRow();
	engine()->document.lines.insert(engine()->document.lines.begin() + m->parsed_row_index, Document::Line(QByteArray()));
	// commit next line
	commitLine(next_line);

	setCursorCol(0);

	clearParsedLine();
	updateVisibility(true, true, true);
}

/**
 * @brief 現在行の桁座標リストを作成する
 * @param out
 */
void AbstractCharacterBasedApplication::makeColumnPosList(std::vector<int> *out)
{
	out->clear();
	std::vector<Char> const *line = &m->parsed_current_line;
	int x = 0;
	while (1) {
		size_t index = out->size();
		out->push_back(x);
		int n;
		uint32_t c = -1;
		if (index < line->size()) {
			c = line->at(index).unicode;
		}
		if (c == '\r' || c == '\n' || c == (uint32_t)-1) {
			break;
		}
		if (c == '\t') { //@
//			int z = nextTabStop(x);
//			n = z - x;
			n = 1;
		} else {
//			n = charWidth(c);
			n = 1;
		}
		x += n;
	}
}

void AbstractCharacterBasedApplication::updateCursorPos(bool auto_scroll)
{
	if (!cx()->engine) {
		return;
	}

	parseCurrentLine(&m->parsed_current_line, 0, false);

	int index = 0;
	int char_span = 0;
	int col = cx()->current_col_hint;

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

	m->parsed_col_index = index;

//	setCurrentCol(col);

	if (char_span < 1) {
		char_span = 1;
	}
	cx()->current_char_span = char_span;

	if (auto_scroll) {
		int pos = decideColumnScrollPos();
		if (cx()->scroll_col_pos != pos) {
			cx()->scroll_col_pos = pos;
			invalidateArea();
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
	text = text.arg(currentRow() + 1).arg(currentCol() + 1);
	return text;
}

int AbstractCharacterBasedApplication::printArea(TextEditorContext const *cx, const SelectionAnchor *sel_a, const SelectionAnchor *sel_b)
{
	int end_of_line_y = -1;
	if (cx) {
		int height = cx->viewport_height;
		QRect clip(cx->viewport_org_x, cx->viewport_org_y, cx->viewport_width, height);
		int row = cx->scroll_row_pos;
		for (int i = 0; i < height; i++) {
			if (row < 0) continue;
			int y = cx->viewport_org_y + i;
			if (row < (int)cx->engine->document.lines.size()) {
				if (i < height) {
					int x = cx->viewport_org_x - cx->scroll_col_pos;
					Document::Line const &line = cx->engine->document.lines[row];
					int anchor_a = -1;
					int anchor_b = -1;
					if (sel_a && sel_a->enabled && sel_b && sel_b->enabled) {
						SelectionAnchor a = *sel_a;
						SelectionAnchor b = *sel_b;
						if (a.row > b.row) {
							std::swap(a, b);
						} else if (a.row == b.row) {
							if (a.col > b.col) {
								std::swap(a, b);
							}
						}
						if (row > a.row && row < b.row) {
							anchor_a = 0;
						} else {
							if (row == a.row) {
								anchor_a = a.col;
							}
							if (row == b.row) {
								anchor_b = b.col;
							}
						}
					}
					QList<FormattedLine2> lines = formatLine_(line, cx->tab_span, anchor_a, anchor_b);
					for (FormattedLine2 const &line : lines) {
						AbstractCharacterBasedApplication::Option opt;
						if (line.atts & FormattedLine2::StyleID) {
							opt.char_attr.color = QColor(line.atts & 0xff, (line.atts >> 8) & 0xff, (line.atts >> 16) & 0xff);
						}
						opt.clip = clip;
						if (line.isSelected()) {
							opt.char_attr.flags |= CharAttr::Selected;
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
				if (y >= 0 && y < (int)m->line_flags.size()) {
					m->line_flags[y] |= LineChanged;
				}
			}
			row++;
		}
	}
	return end_of_line_y;
}

void AbstractCharacterBasedApplication::paintLineNumbers(std::function<void(int, QString const &, Document::Line const *)> const &draw)
{
	auto Line = [&](int index)->Document::Line &{
		return editor_cx->engine->document.lines[index];
	};
	int rightpadding = 2;
	int left_margin = editor_cx->viewport_org_x;
	int num = 1;
	size_t offset = 0;
	for (int i = 0; i <= editor_cx->viewport_height; i++) {
		char tmp[100];
		Q_ASSERT(left_margin < (int)sizeof(tmp));
		memset(tmp, ' ', left_margin);
		tmp[left_margin] = 0;
		int row = editor_cx->scroll_row_pos + i;
		auto LineNumberText = [&](char *ptr, int linenum){
			sprintf(ptr, "%*u ", left_margin - rightpadding, linenum);
		};
		Document::Line *line = nullptr;
		if (row < (int)editor_cx->engine->document.lines.size()) {
			if (m->valid_line_index < 0) {
				m->valid_line_index = 0;
				Document::Line *p = &Line(0);
				if (p->type != Document::Line::Unknown) {
					p->byte_offset = offset;
					p->line_number = num;
					offset += p->text.size();
					num++;
				}
			}
			if (row >= m->valid_line_index) {
				{
					Document::Line const &line2 = Line(m->valid_line_index);
					offset = line2.byte_offset;
					num = line2.line_number;
				}
				while (m->valid_line_index <= row) {
					Document::Line const &line = Line(m->valid_line_index);
					if (line.type != Document::Line::Unknown) {
						offset += line.text.size();
						num++;
					}
					m->valid_line_index++;
					if (m->valid_line_index < editor_cx->engine->document.lines.size()) {
						Document::Line *p = &Line(m->valid_line_index);
						p->byte_offset = offset;
						p->line_number = num;
					}
				}
			}
			if (left_margin > 1) {
				line = &Line(row);
				unsigned int linenum = -1;
				if (row < m->valid_line_index) {
#if 1
					linenum = line->line_number;
#else
					linenum = line->byte_offset;
#endif
				}
				if (linenum != (unsigned int)-1 && line->type != Document::Line::Unknown) {
					LineNumberText(tmp, linenum);
				}
			}
		} else if (row == 0 && editor_cx->engine->document.lines.empty()) {
			LineNumberText(tmp, 1);
		}
		int y = editor_cx->viewport_org_y + i;
		draw(y, tmp, line);
	}
}

bool AbstractCharacterBasedApplication::isAutoLayout() const
{
	return m->auto_layout;
}

void AbstractCharacterBasedApplication::preparePaintScreen()
{
	if (m->header_line > 0) {
		char const *line = "Hello, world\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86";
		printInvertedBar(0, 0, line, ' ');
	}

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
		if (selection_end.enabled != SelectionAnchor::False) {
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

	if (isDialogMode()) {
		printArea(editor_cx.get(), &anchor_a, &anchor_b);

		std::string text = m->dialog_title.toStdString();
		text = ' ' + text + ' ';
		int y = screenHeight() - 2;
		printInvertedBar(3, y, text.c_str(), '-');

		MakeSelectionAnchor();
		printArea(dialog_cx.get(), &anchor_a, &anchor_b);
	} else {
		MakeSelectionAnchor();
		editor_cx->bottom_line_y = printArea(editor_cx.get(), &anchor_a, &anchor_b);

		if (m->footer_line > 0) {
			QString line = statusLine();
			int y = screenHeight() - 1;
			printInvertedBar(0, y, line.toStdString().c_str(), ' ');
		}
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

SelectionAnchor AbstractCharacterBasedApplication::currentAnchor(SelectionAnchor::Enabled enabled)
{
	SelectionAnchor a;
	a.row = currentRow();
	a.col = currentCol();
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

void AbstractCharacterBasedApplication::setSelectionAnchor(SelectionAnchor::Enabled enabled, bool update_anchor, bool auto_scroll)
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
		showHeader(false);
		showFooter(false);
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

bool AbstractCharacterBasedApplication::isBottom() const
{
	if (currentRow() == m->parsed_row_index) {
		if (m->parsed_col_index == (int)m->parsed_current_line.size()) {
			return true;
		}
	}
	return false;
}

void AbstractCharacterBasedApplication::moveToTop()
{
	if (isSingleLineMode()) return;

	deselect();

	setCurrentRow(0);
	setCurrentCol(0);
	cx()->current_col_hint = 0;
	cx()->scroll_row_pos = 0;
	scrollToTop();
	invalidateArea();
	clearParsedLine();
	updateVisibility(true, false, true);
}

void AbstractCharacterBasedApplication::logicalMoveToBottom()
{
	deselect();

	setCurrentRow(documentLines());
	setCurrentCol(0);
	if (currentRow() > 0) {
		setCurrentRow(currentRow() - 1);
		clearParsedLine();
		fetchCurrentLine();
		int col = calcVisualWidth(Document::Line(m->current_line_data));
		setCurrentCol(col);
		cx()->current_col_hint = col;
	}
	cx()->scroll_row_pos = scrollBottomLimit();
}

void AbstractCharacterBasedApplication::logicalMoveToBottom2()
{
	deselect();

	setCurrentRow(documentLines());
	setCurrentCol(0);
	if (currentRow() > 0) {
		setCurrentRow(currentRow() - 1);
		clearParsedLine();
		fetchCurrentLine();
		int col = calcVisualWidth(Document::Line(m->current_line_data));
		setCurrentCol(col);
		cx()->current_col_hint = col;
	}
	cx()->scroll_row_pos = scrollBottomLimit2();
}

void AbstractCharacterBasedApplication::moveToBottom()
{
	if (isSingleLineMode()) return;

	logicalMoveToBottom2();

	invalidateArea();
	clearParsedLine();
	updateVisibility(true, false, true);
}

int AbstractCharacterBasedApplication::findSyntax(QList<Document::CharAttr_> const *list, size_t offset)
{
	int lo = 0;
	int hi = list->size();
	while (lo + 1 < hi) {
		int mid = (lo + hi) / 2;
		Document::CharAttr_ const *a = &list->at(mid);
		if (offset == a->offset) {
			return mid;
		}
		if (offset < a->offset) {
			hi = mid;
		} else {
			lo = mid;
		}
	}
	return lo;
}

void AbstractCharacterBasedApplication::insertSyntax(QList<Document::CharAttr_> *list, size_t offset, Document::CharAttr_ const &a)
{
	int i = findSyntax(list, offset);
	int n = list->size();
	if (i < n) {
		if (list->at(i).offset == offset) {
			(*list)[i] = a;
		} else {
			if (i < n) {
				if (a.offset < list->at(i).offset) {
					if (list->at(i).color == a.color) {
						(*list)[i].offset = offset;
						return;
					}
				}
			}
			if (list->at(i).color == a.color) {
				return;
			}
			if (list->at(i).offset < a.offset) {
				i++;
			}
			list->insert(list->begin() + i, a);
			while (++i < n) {
				(*list)[i].offset++;
			}
		}
	} else {
		list->push_back(a);
	}
}

void AbstractCharacterBasedApplication::internalWrite(const ushort *begin, const ushort *end)
{
	deleteIfSelected();
	clearShiftModifier();

	if (cx()->engine->document.lines.isEmpty()) {
		Document::Line line;
		line.type = Document::Line::Normal;
		line.line_number = 1;
		cx()->engine->document.lines.push_back(line);
	}

	if (!isCurrentLineWritable()) return;

	int len = end - begin;
	parseCurrentLine(nullptr, len, false);
	int index = m->parsed_col_index;
	if (index < 0) {
		addNewLineToBottom();
		index = 0;
	}

	std::vector<Char> *vec = &m->parsed_current_line;

	auto WriteChar = [&](uint32_t c){
		if (isInsertMode()) {
			vec->insert(vec->begin() + index, Char(c));
		} else if (isOverwriteMode()) {
			if (index < (int)vec->size()) {
				uint32_t d = vec->at(index).unicode;
				if (d == '\n' || d == '\r') {
					vec->insert(vec->begin() + index, Char(c));
				} else {
					vec->at(index) = Char(c);
				}
			} else {
				vec->emplace_back(c);
			}
		}
		Document::CharAttr_ a;
		a.offset = index;
		a.color = m->escape_sequence.fg_color_code();
		insertSyntax(&m->syntax_table, index, a);
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
					index++;
				}
			}
		} else {
			WriteChar(c);
			index++;
		}
	}
	m->parsed_col_index = index;
	commitLine(*vec);
	setCursorCol(index);
	updateVisibility(true, true, true);
}

void AbstractCharacterBasedApplication::pressLetterWithControl(int c)
{
	if (c < 0 || c > 0x7f) {
		return;
	}
	if (c < 0x40) {
		c += 0x40;
	}
	c = toupper(c);
	switch (c) {
	case 'Q':
		onQuit();
		break;
	case 'O':
		onOpenFile();
		break;
	case 'S':
		onSaveFile();
		break;
	case 'X':
		editCut();
		break;
	case 'C':
		editCopy();
		break;
	case 'V':
		editPaste();
		break;
	}
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
		} else if (c >= 1 && c <= 26) {
			pressLetterWithControl(c);
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
			if (ok) moveCursorHome();
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

void AbstractCharacterBasedApplication::appendBulk(char const *ptr, int len)
{
	if (!cx()->engine->document.lines.empty()) {
		if (!cx()->engine->document.lines.back().endsWithNewLine()) {
			cx()->engine->document.lines.back().text.append(ptr, len);
			return;
		}
	}
	Document::Line line(QByteArray(ptr, len));
	cx()->engine->document.lines.push_back(line);
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



void Document::retrieveLastText(std::vector<char> *out, int maxlen) const
{
	int remain = maxlen;
	int i = lines.size();
	while (i > 0 && remain > 0) {
		i--;
		QByteArray const &data = lines[i].text;
		int n = data.size();
		if (n > remain) {
			n = remain;
		}
		char const *p = data.data() + data.size() - n;
		out->insert(out->begin(), p, p + n);
		remain -= n;
	}
}

//AbstractCharacterBasedApplication::Char::operator unsigned int() const
//{
//	qDebug();
//	return 'A';
//}


