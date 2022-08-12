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

class TextEditorView : public QWidget, public AbstractTextEditorApplication {
	Q_OBJECT
public:
	class FormattedLine {
	public:
		std::shared_ptr<std::vector<Char>> sp;
		FormattedLine()
			: sp(std::make_shared<std::vector<Char>>())
		{
		}
	};
	class FormattedLines {
	public:
		std::vector<TextEditorView::FormattedLine> lines;
		void clear()
		{
			lines.clear();
		}
		std::vector<Char> *append()
		{
			lines.emplace_back();
			return lines.back().sp.get();
		}
		size_t size() const
		{
			return lines.size();
		}
		FormattedLine &operator [] (size_t i)
		{
			return lines[i];
		}
		FormattedLine const &operator [] (size_t i) const
		{
			return lines[i];
		}
		std::vector<Char> *chars(size_t i)
		{
			return lines[i].sp.get();
		}
		std::vector<Char> const *chars(size_t i) const
		{
			return lines[i].sp.get();
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
	void internalUpdateScrollBar();
	void moveCursorByMouse();
	int calcPixelPosX(int row, int col, bool adjust_scroll, std::vector<Char> *chars) const;
	void parse(int row, std::vector<Char> *chars) const;
	int scrollPosX() const;
	void calcPixelPosX(std::vector<Char> *chars, const QFontMetrics &fm) const;
	int view_y_from_row(int row) const;
public:
	FormattedLines *fetchLines();
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
protected:
	void timerEvent(QTimerEvent *) override;
	void setCursorCol(int col) override;
	void setCursorRow(int row, bool auto_scroll, bool by_mouse) override;
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
	void refrectScrollBar();

	void setRenderingMode(RenderingMode mode);

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
	void updateLayout();

	struct PointInView {
		int x = 0;
		int y = 0;
		int height = 0;
	};
	PointInView pointInView(int row, int col) const;
signals:
	void moved(int cur_row, int cur_col, int scr_row, int scr_col);
	void updateScrollBar();
	void idle();
};




#endif // TEXTEDITORVIEW_H
