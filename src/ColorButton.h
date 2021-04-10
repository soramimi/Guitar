#ifndef COLORBUTTON_H
#define COLORBUTTON_H

#include <QToolButton>

class ColorButton : public QToolButton {
	Q_OBJECT
private:
	QColor color_;
protected:
	void paintEvent(QPaintEvent *event);
public:
	explicit ColorButton(QWidget *parent = nullptr);

	QColor color() const;
	void setColor(const QColor &color);
};

#endif // COLORBUTTON_H
