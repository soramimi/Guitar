#ifndef SIMPLEIMAGEWIDGET_H
#define SIMPLEIMAGEWIDGET_H

#include <QWidget>

class SimpleImageWidget : public QWidget {
	Q_OBJECT
public:
	explicit SimpleImageWidget(QWidget *parent = nullptr);
	void setImage(QImage const &image);
protected:
	void paintEvent(QPaintEvent *event);
	void mouseReleaseEvent(QMouseEvent *event);
private:
	QImage image_;
signals:
	void clicked();
};

#endif // SIMPLEIMAGEWIDGET_H
