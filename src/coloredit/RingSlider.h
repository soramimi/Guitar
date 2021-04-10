#ifndef RINGSLIDER_H
#define RINGSLIDER_H

#include <QSlider>

class RingSlider : public QSlider {
protected:
	int handle_size_ = 16;
	QRect slider_rect_;
	QRect handle_rect_;
	int mouse_press_value_;
	QPoint mouse_press_pos_;
	QImage slider_image_cache_;
	void updateGeometry();
	QSize sliderImageSize() const;
	void offset(int delta);
	void resizeEvent(QResizeEvent *e) override;
	void keyPressEvent(QKeyEvent *e) override;
	void paintEvent(QPaintEvent *) override;
	void mousePressEvent(QMouseEvent *e) override;
	void mouseMoveEvent(QMouseEvent *e) override;
	void mouseDoubleClickEvent(QMouseEvent *e) override;
	void wheelEvent(QWheelEvent *e) override;
	virtual QImage generateSliderImage() = 0;
public:
	explicit RingSlider(QWidget *parent = nullptr)
		: QSlider(parent)
	{}
};


#endif // RINGSLIDER_H
