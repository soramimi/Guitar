#ifndef OREWIDGET_H
#define OREWIDGET_H

#include <QTextFormat>
#include <QWidget>
#include <vector>
#include <cstdint>
#include <memory>
#include "AbstractCharacterBasedApplication.h"
#include "TextEditorTheme.h"

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

class TextEditorWidget : public QWidget, public AbstractTextEditorApplication {
	Q_OBJECT
public:
	enum RenderingMode {
		CharacterMode,
		DecoratedMode,
	};
private:
	struct Private;
	Private *m;

	void paintScreen(QPainter *painter);
	void drawCursor(QPainter *pr);
	void drawFocusFrame(QPainter *pr);
	QRect updateCursorRect(bool auto_scroll);
	RenderingMode renderingMode() const;
	QColor defaultForegroundColor();
	QColor defaultBackgroundColor();
	QColor colorForIndex(CharAttr const &attr, bool foreground);
	void internalUpdateVisibility(bool ensure_current_line_visible, bool change_col, bool auto_scroll);
	void internalUpdateScrollBar();
	void moveCursorByMouse();
	void setTextFont(const QFont &font);
	int defaultCharWidth() const;
	int parseLine3(int row, std::vector<Char> *vec) const;
protected:
	void paintEvent(QPaintEvent *) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void wheelEvent(QWheelEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	void contextMenuEvent(QContextMenuEvent *event) override;
	QFont textFont();
	void drawText(QPainter *painter, int px, int py, QString const &str);
public:
	explicit TextEditorWidget(QWidget *parent = nullptr);
	~TextEditorWidget() override;

	void setTheme(const TextEditorThemePtr &theme);
	TextEditorTheme const *theme() const;

	int charWidth2(unsigned int c) const;
	int lineHeight() const;

	void setPreEditText(PreEditText const &preedit);

	void updateVisibility(bool ensure_current_line_visible, bool change_col, bool auto_scroll) override;

	bool event(QEvent *event) override;

	void bindScrollBar(QScrollBar *vsb, QScrollBar *hsb);
	void setupForLogWidget(QScrollBar *vsb, QScrollBar *hsb, const TextEditorThemePtr &theme);

	QPoint mapFromPixel(const QPoint &pt);
	QPoint mapToPixel(const QPoint &pt);

	QVariant inputMethodQuery(Qt::InputMethodQuery q) const override;
	void inputMethodEvent(QInputMethodEvent *e) override;
	void refrectScrollBar();

	void setRenderingMode(RenderingMode mode);

	void move(int cur_row, int cur_col, int scr_row, int scr_col, bool auto_scroll);
	void layoutEditor() override;
	void setFocusFrameVisible(bool f);
signals:
	void moved(int cur_row, int cur_col, int scr_row, int scr_col);
	void updateScrollBar();
	void idle();
protected:
	void timerEvent(QTimerEvent *) override;
};




#endif // OREWIDGET_H
