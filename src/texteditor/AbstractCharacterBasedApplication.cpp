#include "AbstractCharacterBasedApplication.h"
#include "UnicodeWidth.h"
#include "unicode.h"

#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QFile>
#include "unicode.h"

using WriteMode = AbstractCharacterBasedApplication::WriteMode;
using FormattedLine = AbstractCharacterBasedApplication::FormattedLine;

struct AbstractCharacterBasedApplication::Private {
	bool is_quit_enabled = false;
	bool is_open_enabled = false;
	bool is_save_enabled = false;
	bool is_toggle_selection_anchor_enabled = true;
	bool is_read_only = false;
	State state = State::Normal;
	int header_line = 1;
	int footer_line = 1;
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
	SelectionAnchor selection_anchor;
	int parsed_row_index = -1;
	int parsed_col_index = -1;
	bool parsed_for_edit = false;
	QByteArray parsed_line;
	std::vector<uint32_t> prepared_current_line;
	bool dialog_mode = false;
	DialogHandler dialog_handler;
	bool is_painting_suppressed = false;
	int valid_line_index = 0;
	int line_margin = 3;
	WriteMode write_mode = WriteMode::Insert;
};

AbstractCharacterBasedApplication::AbstractCharacterBasedApplication()
	: m(new Private)
{
}

AbstractCharacterBasedApplication::~AbstractCharacterBasedApplication()
{
	delete m;
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

int AbstractCharacterBasedApplication::leftMargin() const
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

std::vector<AbstractCharacterBasedApplication::Character> *AbstractCharacterBasedApplication::screen()
{
	return &m->screen;
}

const std::vector<AbstractCharacterBasedApplication::Character> *AbstractCharacterBasedApplication::screen() const
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
	editor_cx->viewport_org_x = leftMargin();
	editor_cx->viewport_org_y = m->header_line;
	editor_cx->viewport_width = screenWidth() - cx()->viewport_org_x;
	editor_cx->viewport_height = screenHeight() - (m->header_line + m->footer_line);
}

void AbstractCharacterBasedApplication::initEditor()
{
	editor_cx = std::shared_ptr<TextEditorContext>(new TextEditorContext());
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

QList<FormattedLine> AbstractCharacterBasedApplication::formatLine(Document::Line const &line, int tab_span, int anchor_a, int anchor_b) const
{
	std::vector<ushort> vec;
	vec.reserve(line.text.size() + 100);
	size_t len = line.text.size();
	QList<FormattedLine> res;

	int col = 0;
	int col_start = col;

	bool flag_a = false;
	bool flag_b = false;

	auto Flush = [&](){
		if (!vec.empty()) {
			int atts = 0;
			if (anchor_a >= 0 || anchor_b >= 0) {
				if ((anchor_a < 0 || col_start >= anchor_a) && (anchor_b == -1 || col_start < anchor_b)) {
					atts = 1;
				}
			}
			ushort const *left = &vec[0];
			ushort const *right = left + vec.size();
			while (left < right && (right[-1] == '\r' || right[-1] == '\n')) right--;
			if (left < right) {
				res.push_back(FormattedLine(QString::fromUtf16(left, right - left), atts));
			}
			vec.clear();
		}
		col_start = col;
	};

	if (len > 0) {
		utf8 u8(line.text.data(), len);
		u8.to_utf32([&](uint32_t c){
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
					vec.push_back(c);
				} else {
					int a = c >> 16;
					if (a > 0 && a <= 0x20) {
						a--;
						int b = c & 0x03ff;
						a = (a << 6) | ((c >> 10) & 0x003f);
						a |= 0xd800;
						b |= 0xdc00;
						vec.push_back(a);
						vec.push_back(b);
					}
				}
				col += cw;
			}
			if (anchor_a >= 0 || anchor_b >= 0) {
				if (!flag_a && col >= anchor_a) {
					Flush();
					flag_a = true;
				}
				if (!flag_b && col >= anchor_b) {
					Flush();
					flag_b = true;
				}
			}
			return true;
		});
	}
	Flush();
	return res;
}

void AbstractCharacterBasedApplication::fetchCurrentLine()
{
	QByteArray line;
	const int row = cx()->current_row;
	int lines = documentLines();
	if (row >= 0 && row < lines) {
		line = cx()->engine->document.lines[row].text;
	}
	m->parsed_row_index = row;
	m->parsed_line = line;
}

void AbstractCharacterBasedApplication::clearParsedLine()
{
	m->parsed_row_index = -1;
	m->parsed_for_edit = false;
	m->parsed_line = QByteArray();
}

int AbstractCharacterBasedApplication::cursorX() const
{
	return cx()->current_col - cx()->scroll_col_pos;
}

int AbstractCharacterBasedApplication::cursorY() const
{
	return cx()->current_row - cx()->scroll_row_pos;
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

void AbstractCharacterBasedApplication::commitLine(std::vector<uint32_t> const &vec)
{
	if (isReadOnly()) return;

	QByteArray ba;
	if (!vec.empty()){
		utf32 u32(&vec[0], vec.size());
		u32.to_utf8([&](char c){
			ba.push_back(c);
			return true;
		});
	}
	if (m->parsed_row_index == 0 && engine()->document.lines.empty()) {
		Document::Line newline;
		newline.line_number = 1;
		engine()->document.lines.push_back(newline);
	}
	Document::Line *line = &engine()->document.lines[m->parsed_row_index];
	line->text = ba;

	if (m->valid_line_index > m->parsed_row_index) {
		m->valid_line_index = m->parsed_row_index;
	}

	int y = m->parsed_row_index - cx()->scroll_row_pos + cx()->viewport_org_y;
	if (y >= 0 && y < (int)m->line_flags.size()) {
		m->line_flags[y] |= LineChanged;
	}
}

void AbstractCharacterBasedApplication::parseLine(std::vector<uint32_t> *vec, int increase_hint, bool force)
{
	if (force) {
		clearParsedLine();
	}

	if (force || !m->parsed_for_edit) {
		fetchCurrentLine();
		m->parsed_col_index = internalParseLine(vec ? vec : &m->prepared_current_line, increase_hint);
		m->parsed_for_edit = true;
	} else {
		if (vec) {
			*vec = m->prepared_current_line;
		}
	}
}

QByteArray AbstractCharacterBasedApplication::parsedLine() const
{
	return m->parsed_line;
}

bool AbstractCharacterBasedApplication::isCurrentLineWritable() const
{
	if (isReadOnly()) return false;

	int row = cx()->current_row;
	if (row >= 0 && row < cx()->engine->document.lines.size()) {
		if (cx()->engine->document.lines[row].type != Document::Line::Unknown) {
			return true;
		}
	}
	return false;
}

void AbstractCharacterBasedApplication::internalWrite(const ushort *begin, const ushort *end)
{
	if (cx()->engine->document.lines.isEmpty()) {
		Document::Line line;
		line.type = Document::Line::Normal;
		line.line_number = 1;
		cx()->engine->document.lines.push_back(line);
	}

	if (!isCurrentLineWritable()) return;

	int len = end - begin;
	parseLine(nullptr, len, false);
	int index = m->parsed_col_index;
	if (index < 0) {
		addNewLineToBottom();
		index = 0;
	}

	std::vector<uint32_t> *vec = &m->prepared_current_line;

	auto WriteChar = [&](ushort c){
		if (isInsertMode()) {
			vec->insert(vec->begin() + index, c);
		} else if (isOverwriteMode()) {
			if (index < (int)vec->size()) {
				ushort d = vec->at(index);
				if (d == '\n' || d == '\r') {
					vec->insert(vec->begin() + index, c);
				} else {
					vec->at(index) = c;
				}
			} else {
				vec->push_back(c);
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
	setCursorColByIndex(*vec, index);
	updateVisibility(true, true, true);
}

void AbstractCharacterBasedApplication::write(int c)
{
	if (c < 0x20) {
		if (c == 0x08) {
			doBackspace();
		} else if (c == 0x09) {
			ushort u = c;
			internalWrite(&u, &u + 1);
		} else if (c == 0x0a) {
			pressEnter();
		} else if (c == 0x0d) {
			writeCR();
		} else if (c == 0x1b) {
			pressEscape();
		} else if (c >= 1 && c <= 26) {
			pressLetterWithControl(c);
		}
	} else if (c == 0x7f) {
		doDelete();
	} else if (c < 0x10000) {
		ushort u = c;
		internalWrite(&u, &u + 1);
	} else if (c >= 0x10000 && c <= 0x10ffff) {
		ushort t[2];
		t[0] = (c - 0x10000) / 0x400 + 0xd800;
		t[1] = (c - 0x10000) % 0x400 + 0xdc00;
		internalWrite(t, t + 2);
	} else {
		switch (c) {
		case EscapeCode::Up:
			moveCursorUp();
			break;
		case EscapeCode::Down:
			moveCursorDown();
			break;
		case EscapeCode::Right:
			moveCursorRight();
			break;
		case EscapeCode::Left:
			moveCursorLeft();
			break;
		case EscapeCode::Home:
			moveCursorHome();
			break;
		case EscapeCode::End:
			moveCursorEnd();
			break;
		case EscapeCode::PageUp:
			movePageUp();
			break;
		case EscapeCode::PageDown:
			movePageDown();
			break;
		case EscapeCode::Insert:
			break;
		case EscapeCode::Delete:
			doDelete();
			break;
		}
	}
}

void AbstractCharacterBasedApplication::write(QKeyEvent *e)
{
	int c = e->key();
	if (c == Qt::Key_Backspace) {
		write(0x08);
	} else if (c == Qt::Key_Delete) {
		write(0x7f);
	} else if (c == Qt::Key_Up) {
		if (e->modifiers() & Qt::ControlModifier) {
			scrollUp();
		} else {
			write(EscapeCode::Up);
		}
	} else if (c == Qt::Key_Down) {
		if (e->modifiers() & Qt::ControlModifier) {
			scrollDown();
		} else {
			write(EscapeCode::Down);
		}
	} else if (c == Qt::Key_Left) {
		write(EscapeCode::Left);
	} else if (c == Qt::Key_Right) {
		write(EscapeCode::Right);
	} else if (c == Qt::Key_PageUp) {
		write(EscapeCode::PageUp);
	} else if (c == Qt::Key_PageDown) {
		write(EscapeCode::PageDown);
	} else if (c == Qt::Key_Home) {
		write(EscapeCode::Home);
	} else if (c == Qt::Key_End) {
		write(EscapeCode::End);
	} else if (c == Qt::Key_Return || c == Qt::Key_Enter) {
		write('\n');
	} else if (c == Qt::Key_Escape) {
		write(0x1b);
	} else if (e->modifiers() & Qt::ControlModifier) {
		if (QChar(c).isLetter()) {
			c = QChar(c).toUpper().unicode();
			if (c >= 0x40 && c < 0x60) {
				write(c - 0x40);
			}
		}
	} else {
		QString text = e->text();
		write(text);
	}
}

void AbstractCharacterBasedApplication::write(char const *ptr, int len)
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
			while (left < right) {
				write(*left);
				left++;
			}
			if (c < 0) break;
			right++;
			if (c == '\r') {
				c = isInsertMode() ? '\n' : '\r';
				if (right < end && *right == '\n') {
					c = '\n';
					right++;
				}
				write(c);
			} else if (c == '\n') {
				write('\n');
			}
			left = right;
		} else {
			right++;
		}
	}
}

void AbstractCharacterBasedApplication::write(QString text)
{
	if (isReadOnly()) return;

	if (text.size() == 1) {
		ushort c = text.at(0).unicode();
		write(c);
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
					write(c);
				}
				left = right;
			} else {
				right++;
			}
		}
	}
}

void AbstractCharacterBasedApplication::editPaste()
{
	if (isReadOnly()) return;

	setPaintingSuppressed(true);

#if 1
	QString str = qApp->clipboard()->text();
	utf16(str.utf16(), str.size()).to_utf32([&](uint32_t c){
		write(c);
		return true;
	});
#else
	std::vector<uint32_t> const *source = &m->cut_buffer;
	for (uint32_t c : *source) {
		write(c);
	}
#endif

	setPaintingSuppressed(false);
	updateVisibility(true, true, true);
}

int AbstractCharacterBasedApplication::editorViewportWidth() const
{
	return cx()->viewport_width;
}

int AbstractCharacterBasedApplication::editorViewportHeight() const
{
	return cx()->viewport_height;
}

int AbstractCharacterBasedApplication::print(int x, int y, const QString &text, const AbstractCharacterBasedApplication::Option &opt)
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

void AbstractCharacterBasedApplication::initEngine(std::shared_ptr<TextEditorContext> cx)
{
	cx->engine = TextEditorEnginePtr(new TextEditorEngine);
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

TextEditorEnginePtr AbstractCharacterBasedApplication::engine()
{
	Q_ASSERT(cx()->engine);
	return cx()->engine;
}

void AbstractCharacterBasedApplication::setTextEditorEngine(TextEditorEnginePtr e)
{
	cx()->engine = e;
}

void AbstractCharacterBasedApplication::setDocument(QList<Document::Line> const *source)
{
	document()->lines = *source;
}

void AbstractCharacterBasedApplication::openFile(const QString &path)
{
	document()->lines.clear();
	QFile file(path);
	if (file.open(QFile::ReadOnly)) {
		QString line;
		unsigned int linenum = 0;
		while (!file.atEnd()) {
			linenum++;
			Document::Line line(file.readLine());
			line.type = Document::Line::Normal;
			line.line_number = linenum;
			document()->lines.push_back(line);
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
	scrollToTop();
}

void AbstractCharacterBasedApplication::saveFile(const QString &path)
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
	if (isDialogMode()) {
		closeDialog(true);
	} else {
		writeNewLine();
	}
}

void AbstractCharacterBasedApplication::pressEscape()
{
	if (isDialogMode()) {
		closeDialog(false);
		return;
	}
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
	int top = cx()->current_row - margin;
	int bottom = cx()->current_row + 1 - editorViewportHeight() + margin;
	if (pos > top)    pos = top;
	if (pos < bottom) pos = bottom;
	if (pos < 0) pos = 0;
	if (cx()->scroll_row_pos != pos) {
		cx()->scroll_row_pos = pos;
		invalidateArea();
	}
}

int AbstractCharacterBasedApplication::decideColumnScrollPos() const
{
	int x = cx()->current_col;
	int w = editorViewportWidth() - RIGHT_MARGIN;
	if (w < 0) w = 0;
	return x > w ? (cx()->current_col - w) : 0;
}

int AbstractCharacterBasedApplication::calcVisualWidth(const Document::Line &line) const
{
	QList<FormattedLine> lines = formatLine(line, cx()->tab_span);
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
			if (c == '\t') {
				x += cx()->tab_span;
				x -= x % cx()->tab_span;
			} else {
				x += charWidth(c);
			}
		}
	}
	return x;
}

int AbstractCharacterBasedApplication::xScrollPos() const
{
	return cx()->scroll_col_pos;
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

int AbstractCharacterBasedApplication::calcIndexToColumn(const std::vector<uint32_t> &vec, int index) const
{
	int col = 0;
	for (int i = 0; i < index; i++) {
		uint32_t c = vec.at(i);
		if (c == '\t') {
			col += cx()->tab_span;
			col -= col % cx()->tab_span;
		} else {
			col += charWidth(c);
		}
	}
	return col;
}

void AbstractCharacterBasedApplication::setCursorRow(int row, bool shift, bool auto_scroll)
{
	shift = QApplication::keyboardModifiers() & Qt::ShiftModifier;
	if (shift) {
		if (m->selection_anchor.enabled == SelectionAnchor::No) {
			setSelectionAnchor(SelectionAnchor::EnabledEasy, true, auto_scroll);
		}
	} else if (m->selection_anchor.enabled == SelectionAnchor::EnabledEasy) {
		setSelectionAnchor(SelectionAnchor::No, false, auto_scroll);
	}

	cx()->current_row = row;
}

void AbstractCharacterBasedApplication::setCursorCol(int col, bool shift, bool auto_scroll)
{
	shift = QApplication::keyboardModifiers() & Qt::ShiftModifier;
	if (shift) {
		if (m->selection_anchor.enabled == SelectionAnchor::No) {
			setSelectionAnchor(SelectionAnchor::EnabledEasy, true, auto_scroll);
		}
	} else if (m->selection_anchor.enabled == SelectionAnchor::EnabledEasy) {
		setSelectionAnchor(SelectionAnchor::No, false, auto_scroll);
	}

	cx()->current_col = col;
	cx()->current_col_hint = col;
}

void AbstractCharacterBasedApplication::setCursorPos(int row, int col, bool shift)
{
	setCursorRow(row, shift);
	setCursorCol(col, shift);
}

void AbstractCharacterBasedApplication::setCursorColByIndex(std::vector<uint32_t> const &vec, int col_index)
{
	int col = calcIndexToColumn(vec, col_index);
	setCursorCol(col);
}

int AbstractCharacterBasedApplication::nextTabStop(int x) const
{
	x += cx()->tab_span;
	x -= x % cx()->tab_span;
	return x;
}

void AbstractCharacterBasedApplication::editSelected(EditOperation op, std::vector<uint32_t> *cutbuffer)
{
	if (isReadOnly() && op == EditOperation::Cut) {
		op = EditOperation::Copy;
	}

	SelectionAnchor a = m->selection_anchor;
	SelectionAnchor b = currentAnchor(SelectionAnchor::EnabledHard);
	if (!a.enabled) return;
	if (!b.enabled) return;
	if (a == b) return;

	if (cutbuffer) {
		cutbuffer->clear();
	}
	std::list<std::vector<uint32_t>> cutlist;

	if (a.row > b.row) {
		std::swap(a, b);
	} else if (a.row == b.row) {
		if (a.col > b.col) {
			std::swap(a, b);
		}
	}

	int curr_row = cx()->current_row;
	int curr_col = cx()->current_col;

	cx()->current_row = b.row;
	cx()->current_col = b.col;

	if (a.row == b.row) {
		std::vector<uint32_t> vec1;
		parseLine(&vec1, 0, true);
		auto begin = vec1.begin() + calcColumnToIndex(a.col);
		auto end = vec1.begin() + calcColumnToIndex(b.col);
		if (cutbuffer) {
			std::vector<uint32_t> cut;
			cut.insert(cut.end(), begin, end);
			cutlist.push_back(std::move(cut));
		}
		if (op == EditOperation::Cut) {
			vec1.erase(begin, end);
			commitLine(vec1);
			updateVisibility(true, true, true);
		}
	} else {
		std::vector<uint32_t> vec1;
		parseLine(&vec1, 0, true);
		{
			auto begin = vec1.begin();
			auto end = vec1.begin() + calcColumnToIndex(b.col);
			if (cutbuffer) {
				std::vector<uint32_t> cut;
				cut.insert(cut.end(), begin, end);
				cutlist.push_back(std::move(cut));
			}
			if (op == EditOperation::Cut) {
				vec1.erase(begin, end);
				commitLine(vec1);
				updateVisibility(true, true, true);
			}
		}
		int n = b.row - a.row;
		for (int i = 0; i < n; i++) {
			QList<Document::Line> *lines = &cx()->engine->document.lines;
			if (cutbuffer && i > 0) {
				cx()->current_row = b.row - i;
				cx()->current_col = 0;
				std::vector<uint32_t> cut;
				parseLine(&cut, 0, true);
				cutlist.push_back(std::move(cut));
			}
			if (op == EditOperation::Cut) {
				lines->erase(lines->begin() + b.row - i);
			}
		}

		cx()->current_row = a.row;
		cx()->current_col = a.col;
		int index = calcColumnToIndex(a.col);
		std::vector<uint32_t> vec2;
		parseLine(&vec2, 0, true);
		if (cutbuffer) {
			std::vector<uint32_t> cut;
			cut.insert(cut.end(), vec2.begin() + index, vec2.end());
			cutlist.push_back(std::move(cut));
		}

		if (op == EditOperation::Cut) {
			vec2.resize(index);
			vec2.insert(vec2.end(), vec1.begin(), vec1.end());
			commitLine(vec2);
			updateVisibility(true, true, true);
		}
	}

	if (cutbuffer) {
		size_t size = 0;
		for (std::vector<uint32_t> const &v : cutlist) {
			size += v.size();
		}
		cutbuffer->reserve(size);
		for (auto it = cutlist.rbegin(); it != cutlist.rend(); it++) {
			std::vector<uint32_t> const &v = *it;
			cutbuffer->insert(cutbuffer->end(), v.begin(), v.end());
		}
	}

	deselect();

	if (op == EditOperation::Cut) {
		setCursorPos(a.row, a.col);
		invalidateArea(a.row - cx()->scroll_row_pos);
	} else {
		cx()->current_row = curr_row;
		cx()->current_col = curr_col;
		invalidateArea(curr_row - cx()->scroll_row_pos);
	}

	clearParsedLine();
	updateVisibility(true, true, true);
}

void AbstractCharacterBasedApplication::edit_(EditOperation op)
{
	std::vector<uint32_t> u32buf;
	editSelected(op, &u32buf);
	if (u32buf.empty()) return;

	std::vector<ushort> u16buf;
	u16buf.reserve(1024);
	utf32(&u32buf[0], u32buf.size()).to_utf16([&](uint16_t c){
		u16buf.push_back(c);
		return true;
	});
	if (!u16buf.empty()) {
		QString s = QString::fromUtf16(&u16buf[0], u16buf.size());
		qApp->clipboard()->setText(s);
	}
}

void AbstractCharacterBasedApplication::moveCursorLeft()
{
	if (cx()->current_col == 0) {
		if (isSingleLineMode()) {
			// nop
		} else {
			if (cx()->current_row > 0) {
				setCursorRow(cx()->current_row - 1);
				clearParsedLine();
				moveCursorEnd();
			}
		}
		return;
	}

	int col = 0;
	int newcol = 0;
	int index;
	for (index = 0; index < (int)m->prepared_current_line.size(); index++) {
		ushort c = m->prepared_current_line[index];
		if (c == '\r' || c == '\n') {
			break;
		}
		newcol = col;
		if (c == '\t') {
			col = nextTabStop(col);
		} else {
			col += charWidth(c);
		}
		if (col >= cx()->current_col) {
			break;
		}
	}
	m->parsed_col_index = index;
	setCursorCol(newcol);

	updateVisibility(true, true, true);
}

void AbstractCharacterBasedApplication::doDelete()
{
	if (isReadOnly()) return;

	if (m->selection_anchor.enabled) {
		SelectionAnchor a = currentAnchor(SelectionAnchor::EnabledHard);
		if (m->selection_anchor != a) {
			editSelected(EditOperation::Cut, nullptr);
			return;
		}
	}

	parseLine(nullptr, 0, false);
	std::vector<uint32_t> *vec = &m->prepared_current_line;
	int index = m->parsed_col_index;
	int c = -1;
	if (index >= 0 && index < (int)vec->size()) {
		c = (*vec)[index];
	}
	if (c == '\n' || c == '\r' || c == -1) {
		invalidateAreaBelowTheCurrentLine();
		if (isSingleLineMode()) {
			// nop
		} else {
			if (c != -1) {
				vec->erase(vec->begin() + index);
				if (c == '\r' && index < (int)vec->size() && (*vec)[index] == '\n') {
					vec->erase(vec->begin() + index);
				}
			}
			if (vec->empty()) {
				clearParsedLine();
				if (cx()->current_row < (int)cx()->engine->document.lines.size()) {
					cx()->engine->document.lines.erase(cx()->engine->document.lines.begin() + cx()->current_row);
				}
			} else {
				commitLine(*vec);
				setCursorColByIndex(*vec, index);
				if (index == (int)vec->size()) {
					int nextrow = cx()->current_row + 1;
					int lines = documentLines();
					if (nextrow < lines) {
						Document::Line *ba1 = &cx()->engine->document.lines[cx()->current_row];
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
		setCursorColByIndex(*vec, index);
		updateVisibility(true, true, true);
	}
}

void AbstractCharacterBasedApplication::doBackspace()
{
	if (isReadOnly()) return;

	if (cx()->current_row > 0 || cx()->current_col > 0) {
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

void AbstractCharacterBasedApplication::setDialogOption(QString const &title, QString value, DialogHandler handler)
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
			dialog_cx = std::shared_ptr<TextEditorContext>(new TextEditorContext);
			dialog_cx->engine = std::shared_ptr<TextEditorEngine>(new TextEditorEngine);
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

void AbstractCharacterBasedApplication::execDialog(QString const &dialog_title, QString dialog_value, DialogHandler handler)
{
	setDialogOption(dialog_title, dialog_value, handler);
	setDialogMode(true);
}

void AbstractCharacterBasedApplication::closeDialog(bool result)
{
	if (isDialogMode()) {
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

int AbstractCharacterBasedApplication::internalParseLine(std::vector<uint32_t> *vec, int increase_hint) const
{
	vec->clear();
	int index = -1;
	int col = 0;
	int len = m->parsed_line.size();
	if (len > 0) {
		vec->reserve(len + increase_hint);
		char const *src = m->parsed_line.data();
		utf8 u8(src, len);
		while (1) {
			int n = 0;
			uint32_t c = u8.next();
			if (c == 0) break;
			if (c == '\t') {
				int z = nextTabStop(col);
				n = z - col;
			} else {
				n = charWidth(c);
			}
			if (col <= cx()->current_col && col + n > cx()->current_col) {
				index = (int)vec->size();
			}
			col += n;
			vec->push_back(c);
		}
	}
	return index;
}

int AbstractCharacterBasedApplication::calcColumnToIndex(int column)
{
	int index = 0;
	if (column > 0) {
		fetchCurrentLine();
		int col = 0;
		int len = m->parsed_line.size();
		if (len > 0) {
			char const *src = m->parsed_line.data();
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
	int n = m->line_flags.size();
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

void AbstractCharacterBasedApplication::writeCR()
{
	setCursorCol(0);
	clearParsedLine();
	updateVisibility(true, true, true);
}

void AbstractCharacterBasedApplication::moveCursorHome()
{
	fetchCurrentLine();
	QString line = m->parsed_line;
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
	if (x == cx()->current_col) {
		x = 0;
	}
	setCursorCol(x);
	clearParsedLine();
	updateVisibility(true, true, true);
}

void AbstractCharacterBasedApplication::moveCursorEnd()
{
	fetchCurrentLine();
	QByteArray line = m->parsed_line;
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
	} else if (cx()->current_row > 0) {
		setCursorRow(cx()->current_row - 1);
		clearParsedLine();
		updateVisibility(true, false, true);
	}
}

void AbstractCharacterBasedApplication::moveCursorDown()
{
	if (isSingleLineMode()) {
		// nop
	} else if (cx()->current_row + 1 < (int)document()->lines.size()) {
		setCursorRow(cx()->current_row + 1);
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

void AbstractCharacterBasedApplication::moveCursorRight()
{
	int col = 0;
	int i = 0;
	while (1) {
		int c = -1;
		if (i < (int)m->prepared_current_line.size()) {
			c = m->prepared_current_line[i];
		}
		if (c == '\r' || c == '\n' || c == -1) {
			if (!isSingleLineMode()) {
				int nextrow = cx()->current_row + 1;
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
		if (c == '\t') {
			col = nextTabStop(col);
		} else {
			col += charWidth(c);
		}
		if (col > cx()->current_col) {
			break;
		}
		i++;
	}
	if (col != cx()->current_col) {
		setCursorCol(col);
		clearParsedLine();
		updateVisibility(true, true, true);
	}
}

void AbstractCharacterBasedApplication::movePageUp()
{
	if (!isSingleLineMode()) {
		int step = editorViewportHeight();
		setCursorRow(cx()->current_row - step);
		cx()->scroll_row_pos -= step;
		if (cx()->current_row < 0) {
			cx()->current_row = 0;
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
			setCursorRow(cx()->current_row + step);
			cx()->scroll_row_pos += step;
			if (cx()->current_row > limit) {
				cx()->current_row = limit;
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

void AbstractCharacterBasedApplication::scrollRight()
{
	if (cx()->scroll_col_pos > 0) {
		cx()->scroll_col_pos--;
	} else {
		cx()->scroll_col_pos = 0;
	}
	invalidateArea();
	clearParsedLine();
	updateVisibility(true, true, true);
}

void AbstractCharacterBasedApplication::scrollLeft()
{
	cx()->scroll_col_pos++;
	invalidateArea();
	clearParsedLine();
	updateVisibility(true, true, true);
}

void AbstractCharacterBasedApplication::addNewLineToBottom()
{
	int row = cx()->engine->document.lines.size();
	if (cx()->current_row >= row) {
		setCursorPos(row, 0);
		cx()->engine->document.lines.push_back(Document::Line(QByteArray()));
	}
}

void AbstractCharacterBasedApplication::appendNewLine(std::vector<uint32_t> *vec)
{
	if (isSingleLineMode()) return;

	vec->push_back('\r');
	vec->push_back('\n');
}

void AbstractCharacterBasedApplication::writeNewLine()
{
	if (isReadOnly()) return;
	if (isSingleLineMode()) return;
	if (!isCurrentLineWritable()) return;

	invalidateAreaBelowTheCurrentLine();

	std::vector<uint32_t> curr_line;
	parseLine(&curr_line, 0, false);
	int index = m->parsed_col_index;
	if (index < 0) {
		addNewLineToBottom();
		index = 0;
	}
	std::vector<uint32_t> next_line;
	// split line
	next_line.insert(next_line.end(), curr_line.begin() + index, curr_line.end());
	// shrink current line
	curr_line.resize(index);
	// append new line code
	appendNewLine(&curr_line);
	// next line index
	cx()->current_row = m->parsed_row_index + 1;
	// commit current line
	commitLine(curr_line);
	// insert next line
	m->parsed_row_index = cx()->current_row;
	engine()->document.lines.insert(engine()->document.lines.begin() + m->parsed_row_index, Document::Line(QByteArray()));
	// commit next line
	commitLine(next_line);

	setCursorColByIndex(next_line, 0);

	clearParsedLine();
	updateVisibility(true, true, true);
}

void AbstractCharacterBasedApplication::updateCursorPos(bool auto_scroll)
{
	if (!cx()->engine) {
		return;
	}

	parseLine(&m->prepared_current_line, 0, false);
	std::vector<uint32_t> const *line = &m->prepared_current_line;

	int index = 0;
	int char_span = 0;
	int col = cx()->current_col_hint;
	{
		int x = 0;
		while (1) {
			int n;
			int c = -1;
			if (index < (int)line->size()) {
				c = line->at(index);
			}
			char_span = 0;
			if (c == '\r' || c == '\n' || c == -1) {
				col = x;
				break;
			} else if (c == '\t') {
				int z = nextTabStop(x);
				n = z - x;
			} else {
				n = charWidth(c);
				char_span = n;
			}
			if (x <= col && col < x + n) {
				col = x;
				break;
			}
			x += n;
			index++;
		}
	}

	m->parsed_col_index = index;

	cx()->current_col = col;

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
	text = text.arg(cx()->current_row + 1).arg(cx()->current_col + 1);
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
					QList<FormattedLine> lines = formatLine(line, cx->tab_span, anchor_a, anchor_b);
					for (FormattedLine const &line : lines) {
						AbstractCharacterBasedApplication::Option opt;
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

void AbstractCharacterBasedApplication::paintLineNumbers(std::function<void(int, QString, Document::Line const *line)> draw)
{
	auto Line = [&](int index)->Document::Line &{
		return editor_cx->engine->document.lines[index];
	};
	int left_margin = leftMargin();
	int num = 1;
	for (int i = 0; i < editor_cx->viewport_height; i++) {
		char tmp[100];
		Q_ASSERT(left_margin < sizeof(tmp));
		memset(tmp, ' ', left_margin);
		tmp[left_margin] = 0;
		int row = editor_cx->scroll_row_pos + i;
		Document::Line *line = nullptr;
		if (row < (int)editor_cx->engine->document.lines.size()) {
			line = &Line(row);
			if (row >= m->valid_line_index) {
				num = Line(m->valid_line_index).line_number;
				while (m->valid_line_index <= row) {
					if (Line(m->valid_line_index).type != Document::Line::Unknown) {
						num++;
					}
					m->valid_line_index++;
					if (m->valid_line_index < editor_cx->engine->document.lines.size()) {
						Line(m->valid_line_index).line_number = num;
					}
				}
			}
			if (left_margin > 1) {
				unsigned int linenum = 0;
				if (row < m->valid_line_index) {
					linenum = line->line_number;
				}
				if (linenum > 0 && line->type != Document::Line::Unknown) {
					sprintf(tmp, "%*u ", left_margin - 1, linenum);
				}
			}
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
		paintLineNumbers([&](int y, QString text, Document::Line const *line){
			print(0, y, text + '|', opt_normal);
		});
	}

	SelectionAnchor anchor_a;
	SelectionAnchor anchor_b;

	auto MakeSelectionAnchor = [&](){
		if (m->selection_anchor.enabled != SelectionAnchor::No) {
			anchor_a = m->selection_anchor;
			anchor_b.row = cx()->current_row;
			anchor_b.col = cx()->current_col;
			anchor_b.enabled = m->selection_anchor.enabled;
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
}

SelectionAnchor AbstractCharacterBasedApplication::currentAnchor(SelectionAnchor::Enabled enabled)
{
	SelectionAnchor a;
	a.row = cx()->current_row;
	a.col = cx()->current_col;
	a.enabled = enabled;
	return a;
}

void AbstractCharacterBasedApplication::deselect()
{
	m->selection_anchor.enabled = SelectionAnchor::No;
}

bool AbstractCharacterBasedApplication::isSelectionAnchorEnabled() const
{
	return m->selection_anchor.enabled != SelectionAnchor::No;
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
	return m->is_read_only;
}

void AbstractCharacterBasedApplication::setSelectionAnchor(SelectionAnchor::Enabled enabled, bool update_anchor, bool auto_scroll)
{
	if (update_anchor) {
		m->selection_anchor = currentAnchor(enabled);
	} else {
		m->selection_anchor.enabled = enabled;
	}
	clearParsedLine();
	updateVisibility(false, false, auto_scroll);
}

void AbstractCharacterBasedApplication::toggleSelectionAnchor()
{
	if (!m->is_toggle_selection_anchor_enabled) return;

	setSelectionAnchor(isSelectionAnchorEnabled() ? SelectionAnchor::No : SelectionAnchor::EnabledHard, true, true);
}

void AbstractCharacterBasedApplication::editCopy()
{
	edit_(EditOperation::Copy);
}

void AbstractCharacterBasedApplication::editCut()
{
	edit_(EditOperation::Cut);
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
	case 'A':
		toggleSelectionAnchor();
		break;
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

void AbstractCharacterBasedApplication::setWriteMode(WriteMode wm)
{
	m->write_mode = wm;
}

bool AbstractCharacterBasedApplication::isInsertMode() const
{
	return m->write_mode == WriteMode::Insert;
}

bool AbstractCharacterBasedApplication::isOverwriteMode() const
{
	return m->write_mode == WriteMode::Overwrite;
}

void AbstractCharacterBasedApplication::setTerminalMode()
{
	showHeader(false);
	showFooter(false);
	showLineNumber(false, 0);
	setWriteMode(WriteMode::Overwrite);
	setReadOnly(true);
}

bool AbstractCharacterBasedApplication::isBottom() const
{
	if (cx()->current_row == m->parsed_row_index) {
		if (m->parsed_col_index == m->prepared_current_line.size()) {
			return true;
		}
	}
	return false;
}

void AbstractCharacterBasedApplication::moveToBottom()
{
	if (isSingleLineMode()) return;

	deselect();

	cx()->current_row = documentLines();
	cx()->current_col = 0;
	if (cx()->current_row > 0) {
//		setCursorRow(cx()->current_row - 1);
		cx()->current_row = cx()->current_row - 1;
		clearParsedLine();
		fetchCurrentLine();
//		setCursorCol(calcVisualWidth(Document::Line(m->parsed_line)));
		int col = calcVisualWidth(Document::Line(m->parsed_line));
		cx()->current_col = col;
		cx()->current_col_hint = col;
	}
	cx()->scroll_row_pos = scrollBottomLimit();
	invalidateArea();
	clearParsedLine();
	updateVisibility(true, false, true);
}

