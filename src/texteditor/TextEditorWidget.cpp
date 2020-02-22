#include "TextEditorWidget.h"
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
#include <QTextCodec>
#include <QTimer>

#include "common/misc.h"
#include "InputMethodPopup.h"
#include "unicode.h"

class CharacterSize {
private:
	QFont font_;
	mutable int ascii_[128];
	mutable int height_ = 0;
	mutable std::map<unsigned int, int> map_;
public:
	void reset(QFont const &font)
	{
		font_ = font;
		height_ = 0;
		map_.clear();
		for (int i = 0; i < 128; i++) {
			ascii_[i] = 0;
		}
	}
	int width(unsigned int c) const
	{
		if (height_ == 0) {
			QPixmap pm(1, 1);
			QPainter pr(&pm);
			pr.setFont(font_);
			QFontMetrics fm = pr.fontMetrics();
			height_ = fm.ascent() + fm.descent();
			for (int i = 0; i < 0x20; i++) {
				ascii_[i] = 0;
			}
			for (int i = 0x20; i < 0x80; i++) {
				char tmp[2];
				tmp[0] = i;
				tmp[1] = 0;
				ascii_[i] = fm.size(0, tmp).width();
			}
		}
		if (c < 0x80) {
			return ascii_[c];
		}
		auto it = map_.find(c);
		if (it != map_.end()) {
			return it->second;
		} else {
			QPixmap pm(1, 1);
			QPainter pr(&pm);
			pr.setFont(font_);
			QFontMetrics fm = pr.fontMetrics();
			QString s = QChar(c);
			int w = fm.size(0, s).width();
			map_[c] = w;
			return w;
		}
	}
	int height() const
	{
		if (height_ == 0) {
			width(' ');
		}
		return height_;
	}
};

struct TextEditorWidget::Private {
	PreEditText preedit;
	QFont text_font;
	InputMethodPopup *ime_popup = nullptr;
	int top_margin = 0;
	int bottom_margin = 0;
	CharacterSize character_size;
	int ascent = 0;
	int descent = 0;

	QString status_line;
	QScrollBar *scroll_bar_v = nullptr;
	QScrollBar *scroll_bar_h = nullptr;

	TextEditorThemePtr theme;

	int wheel_delta = 0;

	bool is_focus_frame_visible = false;

	unsigned int idle_count = 0;

	std::function<void(void)> custom_context_menu_requested;
};

TextEditorWidget::TextEditorWidget(QWidget *parent)
	: QWidget(parent)
	, m(new Private)
{
#ifdef Q_OS_WIN
	setTextFont(QFont("MS Gothic", 10));
//	setTextFont(QFont("MS PGothic", 20));
#endif
#ifdef Q_OS_LINUX
	setTextFont(QFont("Monospace", 9));
#endif
#ifdef Q_OS_MACX
	setTextFont(QFontDatabase().font("Osaka", "Regular-Mono", 14));
#endif
#ifdef Q_OS_HAIKU
	setTextFont(QFont("Noto Mono", 9));
#endif

	m->top_margin = 0;
	m->bottom_margin = 1;
	QPixmap pm(1, 1);
	QPainter pr(&pm);
	pr.setFont(textFont());
	QFontMetrics fm = pr.fontMetrics();
	m->ascent = fm.ascent();
	m->descent = fm.descent();

	initEditor();

	setFont(textFont());

	setAttribute(Qt::WA_InputMethodEnabled);
#ifdef Q_OS_WIN
//	m->ime_popup = new InputMethodPopup();
//	m->ime_popup->setFont(font());
//	m->ime_popup->setPreEditText(PreEditText());
#endif

	setContextMenuPolicy(Qt::DefaultContextMenu);

	setRenderingMode(DecoratedMode);

	updateCursorRect(true);

	startTimer(100);
}

TextEditorWidget::~TextEditorWidget()
{
	delete m;
}

void TextEditorWidget::setTheme(TextEditorThemePtr const &theme)
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

void TextEditorWidget::setTextFont(QFont const &font)
{
	m->text_font = font;
	m->character_size.reset(m->text_font);
}

void TextEditorWidget::setRenderingMode(RenderingMode mode)
{
	rendering_mode = mode;
	if (rendering_mode == DecoratedMode) {
		showLineNumber(false);
	} else {
		showLineNumber(true);
	}
	update();
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

int TextEditorWidget::charWidth2(unsigned int c) const
{
	return m->character_size.width(c);
}

int TextEditorWidget::lineHeight() const
{
	return m->character_size.height() + m->top_margin + m->bottom_margin;
}

int TextEditorWidget::defaultCharWidth() const
{
	return m->character_size.width('8');
//	return charWidth2('W');
}

class chars : public abstract_unicode_reader {
private:
	TextEditorWidget::Char const *ptr;
	size_t len;
	size_t pos = 0;
public:
	chars(TextEditorWidget::Char const *ptr, size_t len)
		: ptr(ptr)
		, len(len)
	{
	}
	virtual uint32_t next() override
	{
		if (pos < len) {
			return ptr[pos++].unicode;
		}
		return 0;
	}
};

int TextEditorWidget::parseLine3(int row, int col, std::vector<Char> *vec) const
{
	parseLine2(row, vec);
	QPixmap pm(1, 1);
	QPainter pr(&pm);
	pr.setFont(m->text_font);
	int pos = 0;
	QString s;
	for (size_t i = 0; i < vec->size(); i++) {
		vec->at(i).pos = pos;
		ushort tmp[3];
		uint32_t u = vec->at(i).unicode;
		if (u >= 0x10000 && u < 0x110000) {
			// サロゲートペア
			uint16_t h = (u - 0x10000) / 0x400 + 0xd800;
			uint16_t l = (u - 0x10000) % 0x400 + 0xDC00;
			tmp[0] = h;
			tmp[1] = l;
			tmp[2] = 0;
		} else {
			tmp[0] = u;
			tmp[1] = 0;
		}
		s += QString::fromUtf16(tmp);
		pos = pr.fontMetrics().size(0, s).width();
	}
	if (vec->empty()) {
		pos = 0;
	} else if (col >= 0 && col < (int)vec->size()) {
		pos = vec->at(col).pos;
	} else {
		pos = pos;
	}
	return pos;
}

QPoint TextEditorWidget::mapFromPixel(QPoint const &pt)
{
	const int col = cx()->current_col;
	const int y = pt.y() / lineHeight();
	const int row = y + cx()->scroll_row_pos - cx()->viewport_org_y;
	const int w = defaultCharWidth();
	const int x = pt.x() + (cx()->scroll_col_pos - cx()->viewport_org_x) * w;
	std::vector<Char> vec;
	parseLine3(row, col, &vec);
	for (int col = 0; col + 1 < (int)vec.size(); col++) {
		if (x < (int)vec[col + 1].pos) {
			return QPoint(col, row);
		}
	}
	return QPoint(vec.size(), row);
}

QPoint TextEditorWidget::mapToPixel(QPoint const &pt)
{
	int x = pt.x() * defaultCharWidth();
	int y = pt.y() * lineHeight();
	return QPoint(x, y);
}

void TextEditorWidget::bindScrollBar(QScrollBar *vsb, QScrollBar *hsb)
{
	m->scroll_bar_v = vsb;
	m->scroll_bar_h = hsb;
}

void TextEditorWidget::setupForLogWidget(QScrollBar *vsb, QScrollBar *hsb, TextEditorThemePtr const &theme)
{
	bindScrollBar(vsb, hsb);
	setTheme(theme);
	setAutoLayout(true);
	setTerminalMode(true);
	layoutEditor();
}

QRect TextEditorWidget::updateCursorRect(bool auto_scroll)
{
	updateCursorPos(auto_scroll);

	int x = cx()->viewport_org_x + cursorX();
	int y = cx()->viewport_org_y + cursorY();
	x *= defaultCharWidth();
	y *= lineHeight();
	QPoint pt = QPoint(x, y);
	int w = cx()->current_char_span * defaultCharWidth();
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
		sb->setRange(0, (w + 100) * reference_char_width_);
		sb->setPageStep(w * reference_char_width_);
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

//void TextEditorWidget::setPreEditText(const PreEditText &preedit)
//{
//	m->preedit = preedit;
//	update();
//}

QFont TextEditorWidget::textFont() const
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

QColor TextEditorWidget::colorForIndex(CharAttr const &attr, bool foreground)
{
	if (foreground && attr.color.isValid()) {
		return attr.color;
	}
	switch (attr.index) {
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
		int x2 = 0;
		int x3 = 0;
		while (x < w) {
			std::vector<uint16_t> text;
			text.reserve(w);
			int o = y * w;
			CharAttr charattr;
			Character const *line = &char_screen()->at(o);
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
				x3 += charWidth2(unicode);
				n += cw;
			}
			if (n == 0) {
				n = 1;
				x3 += defaultCharWidth();
			} else if (!text.empty()) {
				QString str = QString::fromUtf16(&text[0], text.size());
				int px = x * defaultCharWidth();
				int py = y * lineHeight();
				px = x2;
				painter->setFont(textFont());
				QFontMetrics fm = painter->fontMetrics();
				int w = fm.width(str);
				int h = lineHeight();
				QColor fgcolor = colorForIndex(charattr, true);
				QColor bgcolor = colorForIndex(charattr, false);
				painter->fillRect(px, py, w, h, bgcolor);
				painter->setPen(fgcolor);

				drawText(painter, px, py, str);
			}
			x += n;
			x2 = x3;
		}
	}
}

void TextEditorWidget::drawFocusFrame(QPainter *pr)
{
	misc::drawFrame(pr, 0, 0, width(), height(), QColor(0, 128, 255, 128));
	misc::drawFrame(pr, 1, 1, width() - 2, height() - 2, QColor(0, 128, 255, 64));
}

#define SCROLL_BY_PIXEL 0
int TextEditorWidget::xScrollPosInPixel()
{
#if SCROLL_BY_PIXEL
	return editor_cx->scroll_col_pos;// * defaultCharWidth();
#else
		return editor_cx->scroll_col_pos * defaultCharWidth();
#endif
}

void TextEditorWidget::drawCursor(QPainter *pr)
{
	std::vector<Char> vec;
	int row = cursorY() + cx()->scroll_row_pos - cx()->viewport_org_y;
	const int col = cx()->current_col;
	int x = parseLine3(row, col, &vec);
	x += cx()->viewport_org_x * defaultCharWidth();
	x -= xScrollPosInPixel();
	const int lineheight = lineHeight();
	const int y = (cx()->viewport_org_y + cursorY()) * lineheight;
	pr->fillRect(x -1, y, 2, lineheight, theme()->fgCursor());
	pr->fillRect(x - 2, y, 4, 2, theme()->fgCursor());
	pr->fillRect(x - 2, y + lineheight - 2, 4, 2, theme()->fgCursor());
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

	auto visualY = [&](TextEditorContext const *context){
		return context->viewport_org_y + editor_cx->current_row - editor_cx->scroll_row_pos;
	};

	// カーソル描画
	if (has_focus) {
		if (isCursorVisible()) {
			// 現在行の下線
			if (renderingMode() == DecoratedMode) {
				int x = cx()->viewport_org_x * defaultCharWidth();
				int y = visualY(cx()) * lineHeight();
				pr.fillRect(x, y, width() - x, lineHeight(), theme()->bgCurrentLine());
				pr.fillRect(x, y + lineHeight() - 1, width() - x, 1, theme()->fgCursor());
			}
			// カーソル
			drawCursor(&pr);
		}
	}

	int linenum_width = editor_cx->viewport_org_x * defaultCharWidth();

	// テキスト描画
	if (renderingMode() == DecoratedMode) {
		int view_row = 0;
		int line_row = editor_cx->scroll_row_pos;
		int lh = lineHeight();
		pr.save();
		pr.setClipRect(linenum_width, 0, width() - linenum_width, height());
		while (isValidRowIndex(line_row)) {
			int x = linenum_width - xScrollPosInPixel();
			int y = view_row * lh;
			if (y >= height()) break;
			QList<FormattedLine> fline = formatLine2(line_row);
			for (FormattedLine const &fl : fline) {
				pr.setPen(defaultForegroundColor());
				pr.drawText(QRect(x, y, width() - linenum_width, lh), fl.text);
			}
			view_row++;
			line_row++;
		}
		pr.restore();
	} else {
		paintScreen(&pr);
	}

	// 行番号描画
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
			if (isCursorVisible() && y == visualY(editor_cx.get())) {
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
					pr.setPen(theme()->fgDefault());
					drawText(&pr, linenum_width - defaultCharWidth() * 3 / 2, y * lineHeight(), mark);
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

	// フォーカス枠描画
	if (m->is_focus_frame_visible && has_focus) {
		drawFocusFrame(&pr);
	}
}

void TextEditorWidget::moveCursorByMouse()
{
	QPoint mousepos = mapFromGlobal(QCursor::pos());
	QPoint pos = mapFromPixel(mousepos);
	int row = pos.y();
	int col = pos.x();

	// row
	if (!isSingleLineMode()) {
		if (row < 0) {
			row = 0;
		} else {
			int maxrow = cx()->engine->document.lines.size();
			maxrow = maxrow > 0 ? (maxrow - 1) : 0;
			if (row > maxrow) {
				row = maxrow;
			}
		}
	}
	setCursorRow(row, false, true);
	setCursorCol(col, false, true);

	clearParsedLine();
	updateVisibility(false, true, false);
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

	moveCursorByMouse();

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

	moveCursorByMouse();

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
				preedit.format.emplace_back(a.start, a.length, f);
			} else {
			}
		}
	}
	if (preedit.text.isEmpty()) {
//		m->ime_popup->hide();
	} else {
		QPoint pt = mapToGlobal(cx()->cursor_rect.topLeft());
//		m->ime_popup->move(pt);
//		m->ime_popup->setFont(font());
//		m->ime_popup->setPreEditText(preedit);
//		m->ime_popup->show();
	}
#endif
//	qDebug() << e->preeditString() << e->commitString();

	QString const &commit_text = e->commitString();
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
		int w = width() / defaultCharWidth();
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



void TextEditorWidget::timerEvent(QTimerEvent *)
{
	if (!isChanged()) {
		m->idle_count++;
		if (m->idle_count >= 10) {
			m->idle_count = 0;
			emit idle();
		}
	}
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




