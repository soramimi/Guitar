#ifndef MYCOMMONSTYLE_H
#define MYCOMMONSTYLE_H

#include <QString>
#include <QStyle>
#include <QStyleOption>

class MyCommonStyleBase {
protected:
	bool x_styleHint(int *value, QStyle::StyleHint hint, const QStyleOption *option, const QWidget *widget, QStyleHintReturn *returnData) const;
	bool x_pixelMetric(int *value, QStyle::PixelMetric metric, const QStyleOption *option, const QWidget *widget) const;
	void x_sizeFromContents(QSize *value, QStyle::ContentsType type, const QStyleOption *option, const QSize &size, const QWidget *widget) const;
public:
	static void drawFrame(QPainter *pr, int x, int y, int w, int h, QColor color_topleft, QColor color_bottomright);
	static void drawShadedFrame(QPainter *p, QRect const &rect, QPalette const &palette, QStyle::State state);
	static void drawFrame(QPainter *pr, const QRect &r, const QColor &color_topleft, const QColor &color_bottomright);
};

template <typename T> class MyCommonStyle : public MyCommonStyleBase, public T {
private:
	using Base = T;
public:
	MyCommonStyle() = default;
	~MyCommonStyle() override = default;
	int styleHint(QStyle::StyleHint hint, const QStyleOption *option, const QWidget *widget = nullptr, QStyleHintReturn *returnData = nullptr) const override
	{
		int value;
		if (x_styleHint(&value, hint, option, widget, returnData)) {
			return value;
		}
		return Base::styleHint(hint, option, widget, returnData);
	}
	int pixelMetric(QStyle::PixelMetric metric, const QStyleOption *option, const QWidget *widget) const override
	{
		int value;
		if (x_pixelMetric(&value, metric, option, widget)) {
			return value;
		}
		return Base::pixelMetric(metric, option, widget);
	}
	QSize sizeFromContents(QStyle::ContentsType type, const QStyleOption *option, const QSize &size, const QWidget *widget) const override
	{
		QSize value = Base::sizeFromContents(type, option, size, widget);
		x_sizeFromContents(&value, type, option, size, widget);
		return value;
	}
};

#endif // MYCOMMONSTYLE_H
