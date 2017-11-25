#include "InputMethodPopup.h"
#include <QBrush>
#include <QDebug>
#include <QPainter>

InputMethodPopup::InputMethodPopup(QWidget *parent)
	: QWidget(parent)
{
	setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
	setFocusPolicy(Qt::NoFocus);
}

void InputMethodPopup::setPreEditText(const PreEditText &preedit)
{
	data.preedit = preedit;

	{
		QPixmap pm(1, 1);
		QPainter pr(&pm);
		pr.setFont(font());
		QFontMetrics fm = pr.fontMetrics();
		int w = fm.size(0, data.preedit.text).width();
		int h = fm.height() + 2;
		setFixedSize(w, h);
	}

	update();
}

void InputMethodPopup::paintEvent(QPaintEvent *)
{
	QPainter pr(this);
	pr.fillRect(0, 0, width(), height(), QColor(192, 192, 192));
#if 1
	if (!data.preedit.text.isEmpty()) {
		QFontMetrics fm = pr.fontMetrics();
		int h = fm.height();
		int x = 0;
		int y = 0;
		for (PreEditText::Format const &f : data.preedit.format) {
			QString s = data.preedit.text.mid(f.start, f.length);
			int w = fm.size(0, s).width();
			if (f.format.hasProperty(QTextFormat::TextUnderlineStyle)) {
				QColor color;
				if (f.format.hasProperty(QTextFormat::TextUnderlineColor)) {
					color = Qt::yellow;//qvariant_cast<QColor>(f.format.property(QTextFormat::TextUnderlineColor));
				}
				if (!color.isValid()) {
					color = Qt::black;
				}
				pr.fillRect(x, y + h, w, 1, Qt::black);
			}
			pr.setPen(QColor(0, 0, 255));
			pr.drawText(x, y + h - 2, s);
			x += w;
		}
	}
#endif
}

void InputMethodPopup::setVisible(bool visible)
{
	QWidget::setVisible(visible);
}
