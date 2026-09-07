#ifndef TEXTEDITORVIEW_H
#define TEXTEDITORVIEW_H

#include "AbstractTextEditorApplication.h"
#include "TextEditorTheme.h"
#include <QTextFormat>
#include <QWidget>
#include <cstdint>
#include <memory>
#include <vector>
#include <functional>

class QScrollBar;

struct PreEditText {
	struct Format {
		int start;
		int length;
		QTextFormat format;
		Format(int start, int length, QTextFormat const &f)
			: start(start)
			, length(length)
			, format(f)
		{
		}
	};

	QString text;
	std::vector<Format> format;
};

class TextEditorView : public QWidget, public AbstractTextEditorApplication {
	Q_OBJECT
public:
	class FormattedLines {
	public:
		int row_start = 0;
		int row_count = 0;
		std::unordered_map<int, TextEditorView::FormattedLine> lines;
		void clear()
		{
			lines.clear();
		}
		size_t size() const
		{
			return lines.size();
		}
	};
private:
	struct Private;
	Private *m;
	
	void drawCursor(int row, int col, QPainter *pr);
	void drawCursor(QPainter *pr);
	void drawFocusFrame(QPainter *pr);
	void update_cursor_rect(bool auto_scroll);
	QColor defaultForegroundColor();
	QColor defaultBackgroundColor();
	QColor colorForIndex(CharAttr const &attr, bool foreground);
	void internalUpdateVisibility(const UpdateVisibilityOption &arg);
public:
	void updateScrollBarRange() override;
private:
	void moveCursorByMouse();
	
	static void _calc_pos_x(CharBuffer *chars, const TextEditorContext *cx, const Font &fixed_tm, const Font &text_tm);
	std::pair<int, int> pos_x_px(row_index_t vrow, col_index_t vcol) const;
	
	int view_y_from_vrow(row_index_t vrow) const;

	QColor cursorColor() const;
	int basic_character_width_px() const;
protected:
	void timerEvent(QTimerEvent *) override;
	void setCursorRow(row_index_t row, bool auto_scroll, bool by_mouse) override;
	
	void calc_pos_x(CharBuffer *chars) const;
	
public:
	const Document::LineProperty *queryFormattedLine(row_index_t vrow) const;
	std::pair<row_index_t, row_index_t> visibleRowAndCount();
	int basisCharWidth() const;
protected:
	void paintEvent(QPaintEvent *) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void wheelEvent(QWheelEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	void contextMenuEvent(QContextMenuEvent *event) override;
	QFont fixedFont() const;
	QFont textFont() const;
	void drawText(QPainter *painter, int px, int py, QString const &str);
	std::pair<int, int> currentPixelX() const;
public:
	explicit TextEditorView(QWidget *parent = nullptr);
	~TextEditorView() override;
	
	void setTheme(const TextEditorThemePtr &theme);
	TextEditorTheme const *theme() const;
	
	void updateVisibility(UpdateVisibilityOption const &arg) override;
	
	bool event(QEvent *event) override;
	
	void bind_scroll_bar(QScrollBar *vsb, QScrollBar *hsb);
	void setup_for_log_widget(const TextEditorThemePtr &theme);
	
	RowCol vpos_from_px(const QPoint &pt);
	
	QVariant inputMethodQuery(Qt::InputMethodQuery q) const override;
	void inputMethodEvent(QInputMethodEvent *e) override;
	void reflectScrollBar();
	
	void move(int cur_row, int cur_col, int scr_y_px, int scr_x_px, bool auto_scroll);
	void layoutEditor() override;
	void setFocusFrameVisible(bool f);
	
	void setFont(const QFont &font)
	{
		setFixedFont(font);
		setTextFont(font);
	}
	
	
	struct PointInView {
		int x = 0;
		int y = 0;
		int height = 0;
	};
	PointInView pointInView(int row, int col) const;
	
	int scrollTopRow() const;
signals:
	void moved(int cur_row, int cur_col, int scr_row, int scr_col);
	void updateScrollBar();
	void idle();
	
public:
	void debug();
	
	// AbstractTextEditorApplication interface
};

#endif // TEXTEDITORVIEW_H
