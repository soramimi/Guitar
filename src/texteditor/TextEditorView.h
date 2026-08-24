#ifndef TEXTEDITORVIEW_H
#define TEXTEDITORVIEW_H

#include "AbstractCharacterBasedApplication.h"
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

class TextMetrics : public AbstractTextMetrics {
public:	
	struct TextWidthCache {
		std::unordered_map<QString, int> map;
	};
	QFont text_font_;
	std::unique_ptr<QFontMetrics> fm_;
	int ascent_ = 0;
	int descent_ = 0;
	QSize basic_character_size_;
	mutable TextWidthCache text_width_cache_;

	void setTextFont(QFont const &font);

	int basisCharWidth() const override;
	int textWidth(QString const &text) const override;
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
	
	void paintScreen(QPainter *painter);
	void drawCursor(int row, int col, QPainter *pr, QColor const &color);
	void drawCursor(QPainter *pr);
	void drawFocusFrame(QPainter *pr);
	void updateCursorRect(bool auto_scroll);
	QColor defaultForegroundColor();
	QColor defaultBackgroundColor();
	QColor colorForIndex(CharAttr const &attr, bool foreground);
	void internalUpdateVisibility(bool ensure_current_line_visible, bool change_col, bool auto_scroll);
public:
	void updateScrollBarRange() override;
private:
	void moveCursorByMouse();
	
	static void _calc_pos_x(std::vector<Character> *chars, const TextEditorContext *cx, const TextMetrics &tm);
        int pos_x_px(row_index_t row, row_index_t col) const;
	
	int scrollPosX() const;
	int view_y_from_row(int row) const;
	int linenumber_area_width() const;

protected:
	void timerEvent(QTimerEvent *) override;
	void setCursorCol(int col) override;
	void setCursorRow(int row, bool auto_scroll, bool by_mouse) override;
	void calc_pos_x(std::vector<Character> *chars) const;
	
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
	QFont textFont() const;
	void drawText(QPainter *painter, int px, int py, QString const &str);
public:
	explicit TextEditorView(QWidget *parent = nullptr);
	~TextEditorView() override;
	
	void setTheme(const TextEditorThemePtr &theme);
	TextEditorTheme const *theme() const;
	
	int lineHeight() const;
	
	void updateVisibility(bool ensure_current_line_visible, bool change_col, bool auto_scroll) override;
	
	bool event(QEvent *event) override;
	
	void bindScrollBar(QScrollBar *vsb, QScrollBar *hsb);
	void setupForLogWidget(const TextEditorThemePtr &theme);
	
	RowCol mapFromPixel(const QPoint &pt);
	
	QVariant inputMethodQuery(Qt::InputMethodQuery q) const override;
	void inputMethodEvent(QInputMethodEvent *e) override;
	void reflectScrollBar();
	
	void move(int cur_row, int cur_col, int scr_row, int scr_col, bool auto_scroll);
	void layoutEditor() override;
	void setFocusFrameVisible(bool f);
	enum ScrollUnit {
		ScrollByCharacter = 0,
	};
	int scroll_unit_ = ScrollByCharacter;
	void setScrollUnit(int n);
	int scrollUnit() const;
	
	void setTextFont(const QFont &font);
	
	struct PointInView {
		int x = 0;
		int y = 0;
		int height = 0;
	};
	PointInView pointInView(int row, int col) const;
	
	int scrollTopRow() const;
	
	std::pair<row_index_t, int> currentVisualPosition();
	std::pair<row_index_t, col_index_t> currentLogicalPosition();
signals:
	void moved(int cur_row, int cur_col, int scr_row, int scr_col);
	void updateScrollBar();
	void idle();
	
};




#endif // TEXTEDITORVIEW_H
