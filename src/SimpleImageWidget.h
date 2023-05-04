#ifndef SIMPLEIMAGEWIDGET_H
#define SIMPLEIMAGEWIDGET_H

#include <QWidget>

class SimpleImageWidget : public QWidget {
	Q_OBJECT
private:
	QImage image_;
protected:
	void paintEvent(QPaintEvent *event);
public:
	explicit SimpleImageWidget(QWidget *parent = nullptr);
	void setImage(QImage const &image);
};

#endif // SIMPLEIMAGEWIDGET_H
