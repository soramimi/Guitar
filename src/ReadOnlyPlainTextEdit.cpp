#include "ReadOnlyPlainTextEdit.h"
#include "main.h"
#include "common/misc.h"

#include <QPainter>

ReadOnlyPlainTextEdit::ReadOnlyPlainTextEdit(QWidget *parent)
	: QPlainTextEdit(parent)
{
	setReadOnly(true);

	int r = panel_bg_color.red();
	int g = panel_bg_color.green();
	int b = panel_bg_color.blue();
	r = r * 4 / 5;
	g = g * 4 / 5;
	b = b * 4 / 5;
	char border[200];
	sprintf(border, "1px solid #%02x%02x%02x", r, g, b);
	QString ss = "* { background: transparent; border: %1; }";
	setStyleSheet(ss.arg(border));
}

void ReadOnlyPlainTextEdit::mousePressEvent(QMouseEvent *event)
{
	// disable drag and drop editting
	QTextCursor c = textCursor();
	c.setPosition(c.selectionEnd());
	setTextCursor(c);

	QPlainTextEdit::mousePressEvent(event);
}

