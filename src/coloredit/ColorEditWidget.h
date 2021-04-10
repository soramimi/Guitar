#ifndef COLOREDITWIDGET_H
#define COLOREDITWIDGET_H

#include <QWidget>

class ColorSquareWidget;

namespace Ui {
class ColorEditWidget;
}

class ColorEditWidget : public QWidget {
	Q_OBJECT
private:
	Ui::ColorEditWidget *ui;
	QColor color_;
	ColorSquareWidget *pickupper_ = nullptr;
	void setColorRed(int value);
	void setColorGreen(int value);
	void setColorBlue(int value);
	void setColorHue(int value);
	void setColorSaturation(int value);
	void setColorValue(int value);
public:
	explicit ColorEditWidget(QWidget *parent = nullptr);
	~ColorEditWidget();
	void bind(ColorSquareWidget *w);

	QColor color() const;
	void setColor(QColor color);
private:
	ColorSquareWidget *pickupper();
private slots:
	void on_horizontalSlider_hsv_h_valueChanged(int value);
	void on_horizontalSlider_hsv_s_valueChanged(int value);
	void on_horizontalSlider_hsv_v_valueChanged(int value);
	void on_horizontalSlider_rgb_r_valueChanged(int value);
	void on_horizontalSlider_rgb_g_valueChanged(int value);
	void on_horizontalSlider_rgb_b_valueChanged(int value);
	void on_spinBox_hsv_h_valueChanged(int arg1);
	void on_spinBox_hsv_s_valueChanged(int arg1);
	void on_spinBox_hsv_v_valueChanged(int arg1);
	void on_spinBox_rgb_r_valueChanged(int arg1);
	void on_spinBox_rgb_g_valueChanged(int arg1);
	void on_spinBox_rgb_b_valueChanged(int arg1);
signals:
	void colorChanged(QColor const &color);
};

#endif // COLOREDITWIDGET_H
