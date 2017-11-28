#include "TextEditorWidget.h"

#include <QDebug>
#include <QFile>
#include <QFontDatabase>
#include <QKeyEvent>
#include <QPainter>
#include <QScrollBar>
#include <QApplication>
#include <functional>
#include <QMenu>

#include "common/misc.h"
#include "InputMethodPopup.h"
#include "unicode.h"

struct TextEditorWidget::Private {
	TextEditorWidget::RenderingMode rendering_mode = TextEditorWidget::CharacterMode;
	PreEditText preedit;
	QFont text_font;
	InputMethodPopup *ime_popup = nullptr;
	int top_margin = 0;
	int bottom_margin = 0;
	int latin1_width = 0;
	int line_height = 0;
	int ascent = 0;
	int descent = 0;

	QString status_line;
	QScrollBar *scroll_bar_v = nullptr;
	QScrollBar *scroll_bar_h = nullptr;

	TextEditorThemePtr theme;

	int wheel_delta = 0;

	bool is_focus_frame_visible = false;

	std::function<void(void)> custom_context_menu_requested;

};

TextEditorWidget::TextEditorWidget(QWidget *parent)
	: QWidget(parent)
	, m(new Private)
{
#ifdef Q_OS_WIN
	m->text_font = QFont("MS Gothic", 10);
#endif
#ifdef Q_OS_LINUX
	m->text_font = QFont("Monospace", 9);
#endif
#ifdef Q_OS_MACX
	m->text_font = QFont("Monaco", 10);
#endif
	m->top_margin = 0;
	m->bottom_margin = 1;
	QPixmap pm(1, 1);
	QPainter pr(&pm);
	pr.setFont(textFont());
	QFontMetrics fm = pr.fontMetrics();
	m->ascent = fm.ascent();
	m->descent = fm.descent();
	m->latin1_width = fm.width('l');
	m->line_height = m->ascent + m->descent + m->top_margin + m->bottom_margin;
	qDebug() << latin1Width() << fm.width("\xe3\x80\x93"); // GETA MARK

	initEditor();

	setFont(textFont());

	setAttribute(Qt::WA_InputMethodEnabled);
#ifdef Q_OS_WIN
	m->ime_popup = new InputMethodPopup();
	m->ime_popup->setFont(font());
	m->ime_popup->setPreEditText(PreEditText());
#endif

	setContextMenuPolicy(Qt::DefaultContextMenu);
//	connect(this, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(onCustomContextMenuRequested(QPoint)));
//	setCustomContextMenuRequestedHandler([&](){
//		defaultCustomContextMenuRequested();
//	});

	setRenderingMode(DecoratedMode);

	updateCursorRect(true);
}

TextEditorWidget::~TextEditorWidget()
{
	delete m;
}

//void TextEditorWidget::setCustomContextMenuRequestedHandler(std::function<void(void)> fn)
//{
//	m->custom_context_menu_requested = fn;
//}



void TextEditorWidget::setTheme(TextEditorThemePtr theme)
{
	m->theme = theme;
}

TextEditorTheme const *TextEditorWidget::theme() const
{
	if (!m->theme) {
		const_cast<TextEditorWidget *>(this)->setTheme(TextEditorTheme::Light());
	}
	return m->theme.get();
}

void TextEditorWidget::setRenderingMode(RenderingMode mode)
{
	m->rendering_mode = mode;
	if (m->rendering_mode == DecoratedMode) {
		showLineNumber(false);
	} else {
		showLineNumber(true);
	}
	update();
}

TextEditorWidget::RenderingMode TextEditorWidget::renderingMode() const
{
	return m->rendering_mode;
}

void AbstractCharacterBasedApplication::loadExampleFile()
{
#ifdef Q_OS_WIN
	QString path = "C:/develop/ore/example.txt";
#elif defined(Q_OS_MAC)
    QString path = "/Users/soramimi/develop/ore/example.txt";
#else
	QString path = "/home/soramimi/develop/ore/example.txt";
#endif
	openFile(path);
}

bool TextEditorWidget::event(QEvent *event)
{
	if (event->type() == QEvent::Polish) {
		clearParsedLine();
		updateVisibility(true, true, true);
	}
	return QWidget::event(event);
}

int TextEditorWidget::latin1Width() const
{
	return m->latin1_width;
}

int TextEditorWidget::lineHeight() const
{
	return m->line_height;
}

QPoint TextEditorWidget::mapFromPixel(QPoint const &pt)
{
	int x = pt.x() / latin1Width();
	int y = pt.y() / lineHeight();
	return QPoint(x, y);
}

QPoint TextEditorWidget::mapToPixel(QPoint const &pt)
{
	int x = pt.x() * latin1Width();
	int y = pt.y() * lineHeight();
	return QPoint(x, y);
}

void TextEditorWidget::moveCursor(QPoint const &pt, bool by_mouse, bool auto_scroll)
{
	if (!isSingleLineMode()) {
		int row = pt.y() - cx()->viewport_org_y + cx()->scroll_row_pos;
		if (row < 0) {
			row = 0;
		} else {
			int maxrow = cx()->engine->document.lines.size();
			maxrow = maxrow > 0 ? (maxrow - 1) : 0;
			if (row > maxrow) {
				row = maxrow;
			}
		}
		setCursorRow(row, false, auto_scroll);
	}
	{
		int col = pt.x();
		if (col < 0) {
			col = 0;
		} else {
			fetchCurrentLine();
			QByteArray line = parsedLine();
			int maxcol = calcVisualWidth(Document::Line(line));
			if (col > maxcol) {
				col = maxcol;
			}
		}
		setCursorCol(col, false, auto_scroll);
	}
	clearParsedLine();
	updateVisibility(!by_mouse, true, auto_scroll);
}

void TextEditorWidget::bindScrollBar(QScrollBar *vsb, QScrollBar *hsb)
{
	m->scroll_bar_v = vsb;
	m->scroll_bar_h = hsb;
}

QRect TextEditorWidget::updateCursorRect(bool auto_scroll)
{
	updateCursorPos(auto_scroll);

	int x = cx()->viewport_org_x + cursorX();
	int y = cx()->viewport_org_y + cursorY();
	x *= latin1Width();
	y *= lineHeight();
	QPoint pt = QPoint(x, y);
	int w = cx()->current_char_span * latin1Width();
	int h = lineHeight();
	cx()->cursor_rect = QRect(pt.x(), pt.y(), w, h);

	QApplication::inputMethod()->update(Qt::ImCursorRectangle);

	return cx()->cursor_rect;
}

void TextEditorWidget::internalUpdateScrollBar()
{
	QScrollBar *sb;

	sb = m->scroll_bar_v;
	if (sb) {
		sb->blockSignals(true);
		sb->setRange(0, document()->lines.size() - cx()->viewport_height / 2);
		sb->setPageStep(editorViewportHeight());
		sb->setValue(cx()->scroll_row_pos);
		sb->blockSignals(false);
	}

	sb = m->scroll_bar_h;
	if (sb) {
		int w = editorViewportWidth();
		sb->blockSignals(true);
		sb->setRange(0, w + 100);
		sb->setPageStep(w);
		sb->setValue(cx()->scroll_col_pos);
		sb->blockSignals(false);
	}

	emit updateScrollBar();
}

void TextEditorWidget::internalUpdateVisibility(bool ensure_current_line_visible, bool change_col, bool auto_scroll)
{
	if (ensure_current_line_visible) {
		ensureCurrentLineVisible();
	}

	updateCursorRect(auto_scroll);

	if (change_col) {
		cx()->current_col_hint = cx()->current_col;
	}

	if (isPaintingSuppressed()) {
		return;
	}

	internalUpdateScrollBar();

	update();
}

void TextEditorWidget::updateVisibility(bool ensure_current_line_visible, bool change_col, bool auto_scroll)
{
	internalUpdateVisibility(ensure_current_line_visible, change_col, auto_scroll);
	emit moved(cx()->current_row, cx()->current_col, cx()->scroll_row_pos, cx()->scroll_col_pos);
}

void TextEditorWidget::move(int cur_row, int cur_col, int scr_row, int scr_col, bool auto_scroll)
{
	if ((cur_row >= 0 && cx()->current_row != cur_row) || (cur_col >= 0 && cx()->current_col != cur_col) || cx()->scroll_row_pos != scr_row || cx()->scroll_col_pos != scr_col) {
		if (cur_row >= 0) cx()->current_row = cur_row;
		if (cur_col >= 0) cx()->current_col = cur_col;
		if (scr_row >= 0) cx()->scroll_row_pos = scr_row;
		if (scr_col >= 0) cx()->scroll_col_pos = scr_col;
		internalUpdateVisibility(false, true, auto_scroll);
	}
}

void TextEditorWidget::setPreEditText(const PreEditText &preedit)
{
	m->preedit = preedit;
	update();
}

QFont TextEditorWidget::textFont()
{
	return m->text_font;
}

void TextEditorWidget::drawText(QPainter *painter, int px, int py, QString const &str)
{
	painter->drawText(px, py + lineHeight() - m->bottom_margin - m->descent, str);
}

QColor TextEditorWidget::defaultForegroundColor()
{
	if (renderingMode() == DecoratedMode) {
		return theme()->fgDefault();
	}
	return Qt::white;
}

QColor TextEditorWidget::defaultBackgroundColor()
{
	if (renderingMode() == DecoratedMode) {
		return theme()->bgDefault();
	}
	return Qt::black;
}

QColor TextEditorWidget::colorForIndex(int index, bool foreground)
{
	switch (index) {
	case CharAttr::Invert:
		return foreground ? defaultBackgroundColor() : defaultForegroundColor();
	}
	return foreground ? defaultForegroundColor() : Qt::transparent;//defaultBackgroundColor();
}

void TextEditorWidget::paintScreen(QPainter *painter)
{
	int w = screenWidth();
	int h = screenHeight();
	for (int y = 0; y < h; y++) {
		int x = 0;
		while (x < w) {
			std::vector<uint16_t> text;
			text.reserve(w);
			int o = y * w;
			CharAttr charattr;
			Character const *line = &screen()->at(o);
			int n = 0;
			while (x + n < w) {
				uint32_t c = line[x + n].c;
				uint32_t d = 0;
				if (c == 0 || c == 0xffff) break;
				if ((c & 0xfc00) == 0xdc00) {
					// surrogate 2nd
					break;
				}
				uint32_t unicode = c;
				if ((c & 0xfc00) == 0xd800) {
					// surrogate 1st
					if (x + n + 1 < w) {
						uint16_t t = line[x + n + 1].c;
						if ((t & 0xfc00) == 0xdc00) {
							d = t;
							unicode = (((c & 0x03c0) + 0x0040) << 10) | ((c & 0x003f) << 10) | (d & 0x03ff);
						} else {
							break;
						}
					} else {
						break;
					}
				}
				int cw = charWidth(unicode);
				if (cw < 1) break;
				if (n == 0) {
					charattr = line[x].a;
				} else if (charattr != line[x + n].a) {
					break;
				}
				if (d == 0) {
					text.push_back(c);
				} else { // surrogate pair
					text.push_back(c);
					text.push_back(d);
				}
				n += cw;
			}
			if (n == 0) {
				n = 1;
			} else if (!text.empty()) {
				QString str = QString::fromUtf16(&text[0], text.size());
				int px = x * latin1Width();
				int py = y * lineHeight();
				painter->setFont(textFont());
				QFontMetrics fm = painter->fontMetrics();
				int w = fm.width(str);
				int h = lineHeight();
				QColor fgcolor = colorForIndex(charattr.index, true);
				QColor bgcolor = colorForIndex(charattr.index, false);
				painter->fillRect(px, py, w, h, bgcolor);
				painter->setPen(fgcolor);
				drawText(painter, px, py, str);
			}
			x += n;
		}
	}
}

void TextEditorWidget::drawCursor(QPainter *pr)
{
	int x = cx()->viewport_org_x + cursorX();
	int y = cx()->viewport_org_y + cursorY();
	if (x < cx()->viewport_org_x || x >= cx()->viewport_org_x + cx()->viewport_width) return;
	if (y < cx()->viewport_org_y || y >= cx()->viewport_org_y + cx()->viewport_height) return;
	x *= latin1Width();
	y *= lineHeight();
	int h = lineHeight();
#if 1
	pr->fillRect(x -1, y, 2, h, theme()->fgCursor());
	pr->fillRect(x - 2, y, 4, 2, theme()->fgCursor());
	pr->fillRect(x - 2, y + h - 2, 4, 2, theme()->fgCursor());
#else
	int w = cx()->current_char_span * latin1Width();
	pr->save();
	pr->setOpacity(0.25);
	pr->fillRect(cx()->cursor_rect, theme()->fgCursor());
	pr->restore();
	misc::drawBox(pr, x, y, w, h, theme()->fgCursor());
#endif
}

void TextEditorWidget::paintEvent(QPaintEvent *)
{
	bool has_focus = hasFocus();

	preparePaintScreen();

	QPainter pr(this);
	pr.fillRect(0, 0, width(), height(), defaultBackgroundColor());

	// diff mode
	if (renderingMode() == DecoratedMode) {
		auto FillRowBackground = [&](int row, QColor const &color){
			int y = editor_cx->viewport_org_y + row - editor_cx->scroll_row_pos;
			y *= lineHeight();
			pr.fillRect(0, y, width(), lineHeight(), color);
		};
		Document const &doc = editor_cx->engine->document;
		for (int i = 0; i < editor_cx->viewport_height; i++) {
			int row = i + editor_cx->scroll_row_pos;
			if (row < doc.lines.size()) {
				if (doc.lines[row].type == Document::Line::Unknown) {
					FillRowBackground(row, theme()->bgDiffUnknown());
				} else if (doc.lines[row].type == Document::Line::Add) {
					FillRowBackground(row, theme()->bgDiffAdd());
				} else if (doc.lines[row].type == Document::Line::Del) {
					FillRowBackground(row, theme()->bgDiffDel());
				}
			}
		}
	}

	int current_y = editor_cx->viewport_org_y + editor_cx->current_row - editor_cx->scroll_row_pos;

	int linenum_width = leftMargin() * latin1Width();

	if (has_focus) {
		// current line
		if (renderingMode() == DecoratedMode) {
			int y = current_y * lineHeight();
			pr.fillRect(linenum_width, y + lineHeight() - 1, width() - linenum_width, 1, theme()->fgCursor());
		}

		drawCursor(&pr);
	}

	paintScreen(&pr);

	if (renderingMode() == DecoratedMode) {
		auto FillLineNumberBG = [&](int y, int h, QColor color){
			pr.fillRect(0, y, linenum_width - 2, h, color);
		};
		int bottom = editor_cx->bottom_line_y;

		int view_y = editor_cx->viewport_org_y;
		int view_h = editor_cx->viewport_height;
		view_y *= lineHeight();
		view_h *= lineHeight();
		FillLineNumberBG(view_y, view_h, theme()->bgLineNumber());

		paintLineNumbers([&](int y, QString text, Document::Line const *line){
			if (bottom >= 0 && y > bottom) return;
			if (y == current_y) {
				FillLineNumberBG(y * lineHeight(), lineHeight(), theme()->bgCurrentLineNumber());
			}
			pr.setBackground(Qt::transparent);
			pr.setPen(theme()->fgLineNumber());
			drawText(&pr, 0, y * lineHeight(), text);
			if (line) {
				char const *mark = nullptr;
				if (line->type == Document::Line::Add) {
					mark = "+";
				} else if (line->type == Document::Line::Del) {
					mark = "-";
				}
				if (mark) {
					pr.setPen(Qt::black);
					drawText(&pr, linenum_width - latin1Width(), y * lineHeight(), mark);
				}
			}
		});

		if (linenum_width > 0) {
			pr.fillRect(0, view_y, 1, view_h, Qt::black);
		}
		pr.fillRect(linenum_width - 2, view_y, 1, view_h, Qt::black);

		if (bottom >= 0) {
			int y = (editor_cx->viewport_org_y + bottom) * lineHeight() + 1;
			pr.fillRect(0, y, width(), 1, Qt::black);
		}
	}

	if (m->is_focus_frame_visible && has_focus) {
		misc::drawFrame(&pr, 0, 0, width(), height(), QColor(0, 128, 255, 128));
		misc::drawFrame(&pr, 1, 1, width() - 2, height() - 2, QColor(0, 128, 255, 64));
	}
}

void TextEditorWidget::mousePressEvent(QMouseEvent *event)
{
	if (event->button() == Qt::RightButton) return;

	savePos();

	bool shift = (event->modifiers() & Qt::ShiftModifier);
	if (shift) {
		if (isSelectionAnchorEnabled()) {
			setSelectionAnchor(SelectionAnchor::EnabledEasy, false, false);
		} else {
			setSelectionAnchor(SelectionAnchor::EnabledEasy, true, false);
		}
	}

	QPoint pos = mapFromGlobal(QCursor::pos());
	pos = mapFromPixel(pos);
	pos.rx() += xScrollPos() - leftMargin();
	moveCursor(pos, true, false);

	if (shift) {
		setSelectionAnchor(SelectionAnchor::EnabledEasy, false, false);
	} else {
		setSelectionAnchor(SelectionAnchor::EnabledEasy, true, false);
	}
	selection_anchor_1 = selection_anchor_0;

	if (isTerminalMode()) {
		clearParsedLine();
		restorePos();
	}
}

void TextEditorWidget::mouseReleaseEvent(QMouseEvent * /*event*/)
{
}

void TextEditorWidget::mouseMoveEvent(QMouseEvent * /*event*/)
{
	savePos();

	QPoint pos = mapFromGlobal(QCursor::pos());
	pos = mapFromPixel(pos);
	pos.rx() += xScrollPos() - leftMargin();
	moveCursor(pos, true, false);

	setSelectionAnchor(SelectionAnchor::EnabledEasy, true, false);

	if (isTerminalMode()) {
		clearParsedLine();
		restorePos();
	}
}

QVariant TextEditorWidget::inputMethodQuery(Qt::InputMethodQuery q) const
{
	if (q == Qt::ImCursorRectangle) {
		QRect r = cx()->cursor_rect;
		return r;
	}
	return QWidget::inputMethodQuery(q);
}

void TextEditorWidget::inputMethodEvent(QInputMethodEvent *e)
{
#ifdef Q_OS_WIN
	PreEditText preedit;
	preedit.text = e->preeditString();
	for (QInputMethodEvent::Attribute const &a : e->attributes()) {
		if (a.type == QInputMethodEvent::TextFormat) {
			QTextFormat f = qvariant_cast<QTextFormat>(a.value);
			if (f.type() == QTextFormat::CharFormat) {
				preedit.format.push_back(PreEditText::Format(a.start, a.length, f));
			} else {
			}
		}
	}
	if (preedit.text.isEmpty()) {
		m->ime_popup->hide();
	} else {
		QPoint pt = mapToGlobal(cx()->cursor_rect.topLeft());
		m->ime_popup->move(pt);
		m->ime_popup->setFont(font());
		m->ime_popup->setPreEditText(preedit);
		m->ime_popup->show();
	}
#endif
	qDebug() << e->preeditString() << e->commitString();

	QString commit_text = e->commitString();
	if (!commit_text.isEmpty()) {
		write_(commit_text, true);
	}
}

void TextEditorWidget::refrectScrollBar()
{
	int v = m->scroll_bar_v ? m->scroll_bar_v->value() : -1;
	int h = m->scroll_bar_h ? m->scroll_bar_h->value() : -1;
	move(-1, -1, v, h, false);
}

void TextEditorWidget::layoutEditor()
{
	if (isAutoLayout()) {
		int h = height() / lineHeight();
		int w = width() / latin1Width();
		setScreenSize(w, h, false);
	}
	AbstractTextEditorApplication::layoutEditor();
}

void TextEditorWidget::resizeEvent(QResizeEvent * /*event*/)
{
	if (isAutoLayout()) {
		layoutEditor();
	}
	internalUpdateScrollBar();
}

void TextEditorWidget::wheelEvent(QWheelEvent *event)
{
	int pos = 0;
	m->wheel_delta += event->delta();
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

void TextEditorWidget::setFocusFrameVisible(bool f)
{
	m->is_focus_frame_visible = f;
}

void TextEditorWidget::contextMenuEvent(QContextMenuEvent *event)
{
	QMenu menu;
	QAction *a_cut = nullptr;
	if (!isReadOnly() && !isTerminalMode()) a_cut = menu.addAction("Cut");
	QAction *a_copy = menu.addAction("Copy");
	QAction *a_paste = nullptr;
	if (!isReadOnly() && !isTerminalMode()) a_paste = menu.addAction("Paste");
	QAction *a = menu.exec(misc::contextMenuPos(this, event));
	if (a) {
		if (a == a_cut) {
			editCut();
			return;
		}
		if (a == a_copy) {
			editCopy();
			return;
		}
		if (a == a_paste) {
			editPaste();
			return;
		}
	}

}




