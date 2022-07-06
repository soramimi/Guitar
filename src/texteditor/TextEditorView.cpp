
#include "TextEditorView.h"
#include "InputMethodPopup.h"
#include "common/misc.h"
#include "unicode.h"
#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QFontDatabase>
#include <QKeyEvent>
#include <QMenu>
#include <QPainter>
#include <QScrollBar>
#include <QTextCodec>
#include <QTimer>
#include <functional>

#define PROPORTIONAL_FONT_SUPPORT 0

struct TextEditorView::Private {
	QPixmap reference_pixmap;

	PreEditText preedit;
	QFont text_font;
    InputMethodPopup *ime_popup = nullptr;
	int top_margin = 0;
	int bottom_margin = 0;
	QSize basic_character_size;
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

TextEditorView::TextEditorView(QWidget *parent)
	: QWidget(parent)
	, m(new Private)
{
	m->reference_pixmap = QPixmap(1, 1);

#ifdef Q_OS_WIN


#if PROPORTIONAL_FONT_SUPPORT
	setTextFont(QFont("MS PGothic", 30));
#else
	setTextFont(QFont("MS Gothic", 20));
#endif

#else

//	QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
	QFont font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
	font.setPointSize(20);
	setTextFont(font);

#endif

	m->top_margin = 0;
	m->bottom_margin = 1;

	initEditor();

	setAttribute(Qt::WA_InputMethodEnabled);
#ifdef Q_OS_WIN
	//	m->ime_popup = new InputMethodPopup();
	//	m->ime_popup->setFont(font());
	//	m->ime_popup->setPreEditText(PreEditText());
#endif

	setContextMenuPolicy(Qt::DefaultContextMenu);

	setRenderingMode(GraphicMode);

	updateCursorRect(true);

	setScrollUnit(ScrollByCharacter);

	startTimer(100);
}

TextEditorView::~TextEditorView()
{
	delete m;
}

void TextEditorView::setTheme(TextEditorThemePtr const &theme)
{
	m->theme = theme;
}

TextEditorTheme const *TextEditorView::theme() const
{
	if (!m->theme) {
		const_cast<TextEditorView *>(this)->setTheme(TextEditorTheme::Light());
	}
	return m->theme.get();
}

void TextEditorView::setTextFont(QFont const &font)
{
	m->text_font = font;

	QPainter pr(&m->reference_pixmap);
	pr.setFont(m->text_font);
	QFontMetrics fm = pr.fontMetrics();
	m->ascent = fm.ascent();
	m->descent = fm.descent();
	m->basic_character_size = fm.size(0, "0");
}

void TextEditorView::updateLayout()
{
	auto SplitLines = [](QByteArray const &ba, std::vector<QByteArray> *out){
		char const *begin = ba.data();
		char const *end = begin + ba.size();
		char const *ptr = begin;
		char const *left = ptr;
		while (1) {
			int c = -1;
			if (ptr < end) {
				c = *ptr;
			}
			if (c == '\n' || c == '\r' || c < 0) {
				if (c == '\r') {
					if (ptr + 1 < end && *ptr == '\n') {
						ptr += 2;
					} else {
						ptr++;
					}
				} else if (c == '\n') {
					ptr++;
				}
				if (ptr > left) {
					out->emplace_back(left, ptr - left);
				}
				if (c < 0) break;
				left = ptr;
			} else {
				ptr++;
			}
		}
	};

	if (!cx()->engine->document.lines.empty()) {
		QList<Document::Line> *doclines = &cx()->engine->document.lines;
		auto index = doclines->size() - 1;
		Document::Line line = doclines->at(index);
		doclines->erase(doclines->begin() + index);
		std::vector<QByteArray> newlines;
		SplitLines(line.text, &newlines);
		for (QByteArray const &ba : newlines) {
			if (ba.isEmpty()) continue;
			doclines->insert(doclines->begin() + index, Document::Line(ba));
			index++;
		}
	}
}

void TextEditorView::setRenderingMode(RenderingMode mode)
{
	rendering_mode_ = mode;
	if (renderingMode() == GraphicMode) {
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
	QString path = "/tmp/example.txt";
#endif
	openFile(path);
}

bool TextEditorView::event(QEvent *event)
{
	if (event->type() == QEvent::Polish) {
		clearParsedLine();
		updateVisibility(true, true, true);
	}
	return QWidget::event(event);
}

/**
 * @brief 行の高さ
 * @return
 */
int TextEditorView::lineHeight() const
{
	int h = m->basic_character_size.height() + m->top_margin + m->bottom_margin;
	return h > 0 ? h : 1;
}

/**
 * @brief 基準文字幅
 * @return
 */
int TextEditorView::basisCharWidth() const
{
	int w = m->basic_character_size.width();
	return w > 0 ? w : 1;
}

/**
 * @brief 全文字のX座標を計算する
 * @param chars
 * @param fm
 */
void TextEditorView::calcPixelPosX(std::vector<Char> *chars, QFontMetrics const &fm)
{
	QString text;
	for (size_t i = 0; i < chars->size(); i++) {
		char16_t tmp[3];
		uint32_t u = chars->at(i).unicode;
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
		text += QString::fromUtf16(tmp);
		chars->at(i).right_x = fm.size(0, text).width();
	}
}

/**
 * @brief 行と桁位置からピクセルX座標を求める
 * @param row
 * @param col
 * @param adjust_scroll
 * @param chars（nullptr可）
 * @return
 */
int TextEditorView::calcPixelPosX(int row, int col, bool adjust_scroll, std::vector<Char> *chars) const
{
	if (!chars) {
		std::vector<Char> tmp; // nullptrなら作業用バッファを作る
		return calcPixelPosX(row, col, adjust_scroll, &tmp);
	}

	parseLine(row, chars); // 行を解析

	{
		QPainter pr(&m->reference_pixmap);
		pr.setFont(m->text_font);
		calcPixelPosX(chars, pr.fontMetrics()); // 文字のX座標を計算
	}

	int x = 0;
	if (col > 0 && col - 1 < (int)chars->size()) {
		x = (int)chars->at(col - 1).right_x;
	}

	if (adjust_scroll) { // 原点とスクロール位置に応じてずらす
		x += cx()->viewport_org_x * basisCharWidth() - scrollPosX();
	}

	return x;
}

/**
 * @brief ビューのピクセル座標から行と桁を求める
 * @param pt
 * @return
 */
RowCol TextEditorView::mapFromPixel(QPoint const &pt)
{
	const int y = pt.y() / lineHeight();
	const int row = y + cx()->scroll_row_pos - cx()->viewport_org_y;
	const int maxrow = documentLines();
	if (row >= maxrow) {
		// 最終行より下だったら、最終行の列数を返す
		RowCol t;
		t.row = maxrow - 1;
		if (maxrow > 0) {
			std::vector<Char> vec;
			parseLine(t.row, &vec);
			if (!vec.empty()) {
				t.col = vec.size();
			}
		}
		return t;
	}
	const int w = basisCharWidth(); // 基準文字幅
	const int x = pt.x() + (cx()->scroll_col_pos - cx()->viewport_org_x) * w;
	std::vector<Char> vec;
	calcPixelPosX(row, -1, false, &vec);

	size_t end = vec.size();
	while (end > 0) {
		if (charWidth(vec[end - 1].unicode) != 0) break;
		end--;
	}
	int left = 0;
	for (size_t col = 0; col < end; col++) {
		int right = vec[col].right_x;
		if (x < right) {
			int l = left - x;
			int r = right - x;
			if (l * l < r * r) {
				return RowCol(row, (int)col);
			} else {
				return RowCol(row, (int)col + 1);
			}
		}
		left = right;
	}
	return RowCol(row, end);
}

/**
 * @brief 行位置を変更する
 * @param row
 * @param auto_scroll
 * @param by_mouse
 */
void TextEditorView::setCursorRow(int row, bool auto_scroll, bool by_mouse)
{
	AbstractCharacterBasedApplication::setCursorRow(row, false, by_mouse);

	// ピクセル座標を更新
	cx()->current_row_pixel_y = (cx()->viewport_org_y + cursorRow()) * lineHeight();

	// ピクセル座標から桁位置を再計算する
	int x = cx()->current_col_pixel_x;
	int y = cx()->current_row_pixel_y;
	auto cr = mapFromPixel({x, y});

	setCurrentCol(cr.col); // 桁位置

	updateSelectionAnchor2(auto_scroll);
}

/**
 * @brief 桁位置を変更する
 * @param col
 */
void TextEditorView::setCursorCol(int col)
{
	AbstractCharacterBasedApplication::setCursorCol(col);

	// 水平ピクセル座標を更新
	auto *chrs = parseCurrentLine(nullptr, 0, true);
	cx()->current_col_pixel_x = calcPixelPosX(currentRow(), currentCol(), true, chrs);
}

void TextEditorView::bindScrollBar(QScrollBar *vsb, QScrollBar *hsb)
{
	m->scroll_bar_v = vsb;
	m->scroll_bar_h = hsb;
}

void TextEditorView::setupForLogWidget(TextEditorThemePtr const &theme)
{
	setTheme(theme);
	setAutoLayout(true);
	setTerminalMode(true);
	layoutEditor();
}

void TextEditorView::updateCursorRect(bool auto_scroll)
{
	updateCursorPos(auto_scroll);

	int x = cx()->viewport_org_x + cursorCol();
	int y = cx()->viewport_org_y + cursorRow();
	x *= basisCharWidth();
	y *= lineHeight();
	QPoint pt = QPoint(x, y);
	int w = cx()->current_char_span * basisCharWidth();
	int h = lineHeight();
	cx()->cursor_rect = QRect(pt.x(), pt.y(), w, h);

	QApplication::inputMethod()->update(Qt::ImCursorRectangle);
}

void TextEditorView::internalUpdateScrollBar()
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

void TextEditorView::internalUpdateVisibility(bool ensure_current_line_visible, bool change_col, bool auto_scroll)
{
	if (ensure_current_line_visible) {
		ensureCurrentLineVisible();
	}

	updateCursorRect(auto_scroll);

	if (change_col) {
		cx()->current_col_hint = currentCol();
	}

	if (isPaintingSuppressed()) {
		return;
	}

	internalUpdateScrollBar();

	update();
}

void TextEditorView::updateVisibility(bool ensure_current_line_visible, bool change_col, bool auto_scroll)
{
	internalUpdateVisibility(ensure_current_line_visible, change_col, auto_scroll);
	emit moved(currentRow(), currentCol(), cx()->scroll_row_pos, cx()->scroll_col_pos);
}

void TextEditorView::move(int cur_row, int cur_col, int scr_row, int scr_col, bool auto_scroll)
{
	if ((cur_row >= 0 && currentRow() != cur_row) || (cur_col >= 0 && currentCol() != cur_col) || cx()->scroll_row_pos != scr_row || cx()->scroll_col_pos != scr_col) {
		if (cur_row >= 0) setCurrentRow(cur_row);
		if (cur_col >= 0) setCurrentCol(cur_col);
		if (scr_row >= 0) cx()->scroll_row_pos = scr_row;
		if (scr_col >= 0) cx()->scroll_col_pos = scr_col;
		internalUpdateVisibility(false, true, auto_scroll);
	}
}

QFont TextEditorView::textFont() const
{
	return m->text_font;
}

void TextEditorView::drawText(QPainter *painter, int px, int py, QString const &str)
{
	painter->drawText(px, py + lineHeight() - m->bottom_margin - m->descent, str);
}

QColor TextEditorView::defaultForegroundColor()
{
	if (renderingMode() == GraphicMode) {
		return theme()->fgDefault();
	}
	return Qt::white;
}

QColor TextEditorView::defaultBackgroundColor()
{
	if (renderingMode() == GraphicMode) {
		return theme()->bgDefault();
	}
	return Qt::black;
}

QColor TextEditorView::colorForIndex(CharAttr const &attr, bool foreground)
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

/**
 * @brief 描画（CharacterMode用）
 * @param painter
 */
void TextEditorView::paintScreen(QPainter *painter)
{
	int cols = screenWidth();
	int rows = screenHeight();
	for (int row = 0; row < rows; row++) {
		int col = 0;
		std::vector<Char> text32;
		while (col < cols) {
			int o = row * cols;
			Character const *line = &char_screen()->at(o);
			int n = 0;
			while (col + n < cols) {
				uint32_t c = line[col + n].c;
				uint32_t d = 0;
				if (c == 0) break;
				if (c == 0xffff) break;
				if ((c & 0xfc00) == 0xdc00) {
					// surrogate 2nd
					break;
				}
				uint32_t unicode = c;
				if ((c & 0xfc00) == 0xd800) {
					// surrogate 1st
					if (col + n + 1 < cols) {
						uint16_t t = line[col + n + 1].c;
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
				Char ch;
				int cw = charWidth(unicode);
				if (cw < 1) break;
				if (n == 0) {
					ch.attr = line[col].a;
				} else if (ch.attr != line[col + n].a) {
					break;
				}
				ch.unicode = unicode;
				text32.push_back(ch);
				n += cw;
			}
			if (n == 0) {
				Char ch;
				ch.unicode = ' ';
				text32.push_back(ch);
				n = 1;
			}
			col += n;
		}
		calcPixelPosX(&text32, painter->fontMetrics());

		int px = 0;
		for (size_t i = 0; i < text32.size(); i++) {
			QString str;
			Char const &ch = text32[i];
			uint32_t c = ch.unicode;
			uint32_t d = 0;
			if (c >= 0x010000 && c < 0x110000) {
				d = ((c - 0x010000) & 0x03ff) + 0xdc00;
				c = ((c - 0x010000) >> 10) + 0xd800;
				str.append(QChar((ushort)c));
				str.append(QChar((ushort)d));
			} else {
				str.append(QChar((ushort)c));
			}
			int py = row * lineHeight();
			int w = painter->fontMetrics().boundingRect(str).width();
			int h = lineHeight();
			QColor fgcolor = colorForIndex(ch.attr, true);
			QColor bgcolor = colorForIndex(ch.attr, false);
			painter->fillRect(px, py, w, h, bgcolor);
			painter->setPen(fgcolor);
			drawText(painter, px, py, str);
			px = ch.right_x;
		}
	}
}

void TextEditorView::drawFocusFrame(QPainter *pr)
{
	misc::drawFrame(pr, 0, 0, width(), height(), QColor(0, 128, 255, 128));
	misc::drawFrame(pr, 1, 1, width() - 2, height() - 2, QColor(0, 128, 255, 64));
}

void TextEditorView::setScrollUnit(int n)
{
	scroll_unit_ = n;
}

int TextEditorView::scrollUnit() const
{
	return scroll_unit_;
}

/**
 * @brief 水平スクロール位置のピクセル値を取得
 * @return
 */
int TextEditorView::scrollPosX() const
{
	int u = scrollUnit();
	int n = editor_cx->scroll_col_pos;
	n *= (u == ScrollByCharacter) ? basisCharWidth() : u;
	return n;
}

int TextEditorView::view_y_from_row(int row) const
{
	return (editor_cx->viewport_org_y + row - editor_cx->scroll_row_pos) * lineHeight();
}



/**
 * @brief 行と桁からビュー座標を求める
 * @param row
 * @param col
 * @return
 */
TextEditorView::PointInView TextEditorView::pointInView(int row, int col) const
{
	PointInView pt;
	pt.height = lineHeight();
	pt.y = view_y_from_row(row);
	pt.x = calcPixelPosX(row, col, true, nullptr); // 行と桁位置から水平座標を求める
	return pt;
}

/**
 * @brief カーソルを描画
 * @param row
 * @param col
 * @param pr
 * @param color
 */
void TextEditorView::drawCursor(int row, int col, QPainter *pr, QColor const &color)
{
	PointInView pt = pointInView(row, col);
	pr->fillRect(pt.x -1, pt.y, 2, pt.height, color);
	pr->fillRect(pt.x - 2, pt.y, 4, 2, color);
	pr->fillRect(pt.x - 2, pt.y + pt.height - 2, 4, 2, color);
}

/**
 * @brief 現在位置にカーソルを描画
 * @param pr
 * @param color
 */
void TextEditorView::drawCursor(QPainter *pr)
{
	drawCursor(currentRow(), currentCol(), pr, theme()->fgCursor());
}

/**
 * @brief 描画
 */
void TextEditorView::paintEvent(QPaintEvent *)
{
	bool has_focus = hasFocus();

	preparePaintScreen();

	QPainter pr(this);
	pr.setFont(m->text_font);
	pr.fillRect(0, 0, width(), height(), defaultBackgroundColor());

	// diff mode
	if (renderingMode() == GraphicMode) {
		auto FillRowBackground = [&](int row, QColor const &color){
			int y = view_y_from_row(row);
			pr.fillRect(0, y, width(), lineHeight(), color);
		};
		Document const &doc = editor_cx->engine->document;
		for (int i = 0; i < editor_cx->viewport_height; i++) {
			int row = i + editor_cx->scroll_row_pos;
			if (row >= 0 && row < doc.lines.size()) {
				auto type = doc.lines[row].type;
				if (type == Document::Line::Unknown) {
					FillRowBackground(row, theme()->bgDiffUnknown());
				} else if (type == Document::Line::Add) {
					FillRowBackground(row, theme()->bgDiffAdd());
				} else if (type == Document::Line::Del) {
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
			if (renderingMode() == GraphicMode) {
				int x = cx()->viewport_org_x * basisCharWidth();
				int y = visualY(cx()) * lineHeight();
				pr.fillRect(x, y, width() - x, lineHeight(), theme()->bgCurrentLine());
				pr.fillRect(x, y + lineHeight() - 1, width() - x, 1, theme()->fgCursor());
			}
			// カーソル
			drawCursor(&pr);
		}
	}

	if (0) {
		if (selection_end.enabled != SelectionAnchor::False) {
			int row = selection_end.row;
			int col = selection_end.col;
			drawCursor(row, col, &pr, Qt::red);
		}
		if (selection_start.enabled != SelectionAnchor::False) {
			int row = selection_start.row;
			int col = selection_start.col;
			drawCursor(row, col, &pr, Qt::green);
		}
	}

	int linenum_width = editor_cx->viewport_org_x * basisCharWidth();

	// テキスト描画
	if (renderingMode() == GraphicMode) {
		int view_row = 0;
		int line_row = editor_cx->scroll_row_pos;
		int lh = lineHeight();
		pr.save();
		pr.setClipRect(linenum_width, 0, width() - linenum_width, height());
		QTextOption opt;
		opt.setWrapMode(QTextOption::NoWrap);

		auto selmin = selection_end;
		auto selmax = selection_start;
		if (selmin.enabled == SelectionAnchor::True && selmax.enabled == SelectionAnchor::True) {
			if (selmin.row > selmax.row || (selmin.row == selmax.row && selmin.col > selmax.col)) {
				std::swap(selmin, selmax);
			}
		} else {
			selmin = {};
			selmax = {};
		}

		while (isValidRowIndex(line_row)) {
			int x = linenum_width - scrollPosX();
			int y = view_row * lh;
			if (y >= height()) break;
			QList<FormattedLine> fline = formatLine2(line_row);

			if (1) { // 選択領域(wip)
				std::vector<Char> chars;
				parseLine(line_row, &chars);
				calcPixelPosX(&chars, pr.fontMetrics());
				int left_x = 0;
				int right_x = 0;
				if (!chars.empty()) {
					right_x = chars.back().right_x;
				}
				if (selmin.row > line_row) {
					right_x = 0;
				} else if (selmax.row < line_row) {
					right_x = 0;
				} else {
					if (selmin.row == line_row) {
						left_x = (selmin.col > 0 && selmin.col - 1 < chars.size()) ? chars[selmin.col - 1].right_x : 0;
					}
					if (selmax.row == line_row) {
						right_x = (selmax.col > 0 && selmax.col - 1 < chars.size()) ? chars[selmax.col - 1].right_x : 0;
					}
				}
				if (left_x < right_x) {
					int w =  right_x - left_x;
					pr.fillRect(x + left_x, y, w, lh, QBrush(QColor(64, 192, 192), Qt::Dense5Pattern));
				}
			}

			for (FormattedLine const &fl : fline) {
				pr.setPen(defaultForegroundColor());
				pr.drawText(QRect(x, y, width() - x, lh), fl.text, opt);
			}
			view_row++;
			line_row++;
		}
		pr.restore();
	} else {
		paintScreen(&pr);
	}

	// 行番号描画
	if (renderingMode() == GraphicMode) {
		auto FillLineNumberBG = [&](int y, int h, QColor const &color){
			pr.fillRect(0, y, linenum_width - 2, h, color);
		};
		int bottom = editor_cx->bottom_line_y;

		int view_y = editor_cx->viewport_org_y;
		int view_h = editor_cx->viewport_height;
		view_y *= lineHeight();
		view_h *= lineHeight();
		FillLineNumberBG(view_y, view_h, theme()->bgLineNumber());

		paintLineNumbers([&](int y, QString const &text, Document::Line const *line){
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
					drawText(&pr, linenum_width - basisCharWidth() * 3 / 2, y * lineHeight(), mark);
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

void TextEditorView::moveCursorByMouse()
{
	QPoint mousepos = mapFromGlobal(QCursor::pos()); // マウス座標をグローバルからローカルへ変換
	RowCol pos = mapFromPixel(mousepos); // ローカルマウス座標からカーソル座標へ変換

	// row
	if (!isSingleLineMode()) {
		if (pos.row < 0) {
			pos.row = 0;
		} else {
			int maxrow = cx()->engine->document.lines.size();
			maxrow = maxrow > 0 ? (maxrow - 1) : 0;
			if (pos.row > maxrow) {
				pos.row = maxrow;
			}
		}
	}
	setCursorPosByMouse(pos, mousepos);

	clearParsedLine();
	updateVisibility(false, true, false);
}

void TextEditorView::mousePressEvent(QMouseEvent *event)
{
	if (event->button() == Qt::RightButton) return;

	savePos();

	bool shift = (event->modifiers() & Qt::ShiftModifier);
	if (shift) {
		if (hasSelection()) {
			setSelectionAnchor(SelectionAnchor::True, false, false);
		} else {
			setSelectionAnchor(SelectionAnchor::True, true, false);
		}
	}

	moveCursorByMouse();

	if (shift) {
		setSelectionAnchor(SelectionAnchor::True, false, false);
	} else {
		setSelectionAnchor(SelectionAnchor::True, true, false);
	}
	selection_start = selection_end;

	if (isTerminalMode()) {
		clearParsedLine();
		restorePos();
	}
}

void TextEditorView::mouseReleaseEvent(QMouseEvent * /*event*/)
{
	if (selection_end.enabled == SelectionAnchor::False || selection_start.enabled == SelectionAnchor::False) {
		// thru
	} else if (selection_end.row == selection_start.row && selection_end.col == selection_start.col) {
		// thru
	} else {
		return;
	}
	deselect();
	update();
}

void TextEditorView::mouseMoveEvent(QMouseEvent * /*event*/)
{
	savePos();

	moveCursorByMouse();

	setSelectionAnchor(SelectionAnchor::True, true, false);

	if (isTerminalMode()) {
		clearParsedLine();
		restorePos();
	}
}

QVariant TextEditorView::inputMethodQuery(Qt::InputMethodQuery q) const
{
	if (q == Qt::ImCursorRectangle) {
		QRect r = cx()->cursor_rect;
		return r;
	}
	return QWidget::inputMethodQuery(q);
}

void TextEditorView::inputMethodEvent(QInputMethodEvent *e)
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
	QString const &commit_text = e->commitString();
	if (!commit_text.isEmpty()) {
		write_(commit_text, true);
	}
}

void TextEditorView::refrectScrollBar()
{
	int v = m->scroll_bar_v ? m->scroll_bar_v->value() : -1;
	int h = m->scroll_bar_h ? m->scroll_bar_h->value() : -1;
	move(-1, -1, v, h, false);
}

void TextEditorView::layoutEditor()
{
	if (isAutoLayout()) {
		int h = height() / lineHeight();
		int w = width() / basisCharWidth();
		setScreenSize(w, h, false);
	}
	AbstractTextEditorApplication::layoutEditor();
}

void TextEditorView::resizeEvent(QResizeEvent * /*event*/)
{
	if (isAutoLayout()) {
		layoutEditor();
	}
	internalUpdateScrollBar();
}

void TextEditorView::wheelEvent(QWheelEvent *event)
{
	int pos = 0;
	m->wheel_delta += event->angleDelta().y();
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

void TextEditorView::setFocusFrameVisible(bool f)
{
	m->is_focus_frame_visible = f;
}

void TextEditorView::timerEvent(QTimerEvent *)
{
	if (!isChanged()) {
		m->idle_count++;
		if (m->idle_count >= 10) {
			m->idle_count = 0;
			emit idle();
		}
	}
}



void TextEditorView::contextMenuEvent(QContextMenuEvent *event)
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




