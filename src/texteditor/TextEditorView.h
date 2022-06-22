#ifndef TEXTEDITORVIEW_H
#define TEXTEDITORVIEW_H

#include "AbstractCharacterBasedApplication.h"
#include "TextEditorTheme.h"
#include <QTextFormat>
#include <QWidget>
#include <cstdint>
#include <memory>
#include <vector>

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
private:
	struct Private;
	Private *m;

	void paintScreen(QPainter *painter);
	void drawCursor(QPainter *pr);
	void drawFocusFrame(QPainter *pr);
	void updateCursorRect(bool auto_scroll);
	QColor defaultForegroundColor();
	QColor defaultBackgroundColor();
	QColor colorForIndex(CharAttr const &attr, bool foreground);
	void internalUpdateVisibility(bool ensure_current_line_visible, bool change_col, bool auto_scroll);
	void internalUpdateScrollBar();
	void moveCursorByMouse();
	int calcPixelPosX(int row, int col, bool adjust_scroll, std::vector<Char> *vec_out) const;
	int scrollPosX() const;
	int calcPixelPosX2(const QFontMetrics &fm, int row, int col, std::vector<Char> *vec_out) const;
	int view_y_from_row(int row);
	bool isProportional() const;
public:
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
	void setCursorCol(int col);
	void setCursorRow(int row, bool auto_scroll, bool by_mouse);
public:
	explicit TextEditorView(QWidget *parent = nullptr);
	~TextEditorView() override;

	void setTheme(const TextEditorThemePtr &theme);
	TextEditorTheme const *theme() const;

//	int charWidth2(unsigned int c) const;
	int lineHeight() const;

//	void setPreEditText(PreEditText const &preedit);

	void updateVisibility(bool ensure_current_line_visible, bool change_col, bool auto_scroll) override;

	bool event(QEvent *event) override;

	void bindScrollBar(QScrollBar *vsb, QScrollBar *hsb);
	void setupForLogWidget(const TextEditorThemePtr &theme);

	RowCol mapFromPixel(const QPoint &pt);
	QPoint mapToPixel(const RowCol &pt);

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

	void moveCursorDown();
	void setTextFont(const QFont &font);
signals:
	void moved(int cur_row, int cur_col, int scr_row, int scr_col);
	void updateScrollBar();
	void idle();
};




#endif // TEXTEDITORVIEW_H
