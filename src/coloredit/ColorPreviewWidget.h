#ifndef COLORPREVIEWWIDGET_H
#define COLORPREVIEWWIDGET_H

#include <QWidget>

class ColorPreviewWidget : public QWidget {
private:
	QColor color_;
protected:
	void paintEvent(QPaintEvent *event);
public:
	ColorPreviewWidget(QWidget *parent);
	void setColor(QColor const &color);
};

#endif // COLORPREVIEWWIDGET_H
