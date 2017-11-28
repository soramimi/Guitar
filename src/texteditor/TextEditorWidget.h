#ifndef OREWIDGET_H
#define OREWIDGET_H

#include <QTextFormat>
#include <QWidget>
#include <vector>
#include <stdint.h>
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
	QRect updateCursorRect(bool auto_scroll);
	RenderingMode renderingMode() const;
	QColor defaultForegroundColor();
	QColor defaultBackgroundColor();
	QColor colorForIndex(int index, bool foreground);
	void internalUpdateVisibility(bool ensure_current_line_visible, bool change_col, bool auto_scroll);
	void internalUpdateScrollBar();
protected:
	void paintEvent(QPaintEvent *);
	void keyPressEvent(QKeyEvent *event);
	void mousePressEvent(QMouseEvent *event);
	void mouseReleaseEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
	void wheelEvent(QWheelEvent *event);
	void resizeEvent(QResizeEvent *event);
	void contextMenuEvent(QContextMenuEvent *event);
	QFont textFont();
	void drawText(QPainter *painter, int px, int py, const QString &str);
public:
	explicit TextEditorWidget(QWidget *parent = 0);
	~TextEditorWidget();

	void setTheme(TextEditorThemePtr theme);
	TextEditorTheme const *theme() const;

	int latin1Width() const;
	int lineHeight() const;

	void setPreEditText(PreEditText const &preedit);

	void updateVisibility(bool ensure_current_line_visible, bool change_col, bool auto_scroll);

	bool event(QEvent *event);
	void bindScrollBar(QScrollBar *vsb, QScrollBar *hsb);
	QPoint mapFromPixel(const QPoint &pt);
	QPoint mapToPixel(const QPoint &pt);

	void moveCursor(const QPoint &pt, bool by_mouse, bool auto_scroll);
	QVariant inputMethodQuery(Qt::InputMethodQuery q) const;
	void inputMethodEvent(QInputMethodEvent *e);
	void refrectScrollBar();

	void setRenderingMode(RenderingMode mode);

	void move(int cur_row, int cur_col, int scr_row, int scr_col, bool auto_scroll);
	void layoutEditor();
	void setFocusFrameVisible(bool f);
signals:
	void moved(int cur_row, int cur_col, int scr_row, int scr_col);
	void updateScrollBar();
private slots:
//	void onCustomContextMenuRequested(QPoint);


	// QWidget interface
protected:
};




#endif // OREWIDGET_H
