#ifndef DARKSTYLE_H
#define DARKSTYLE_H

#include "MyCommonStyle.h"

#include <QCommonStyle>

class QStyleOptionViewItem;

class ButtonImages {
public:
	QImage im_normal;
	QImage im_pressed;
};

class DarkStyle : public MyCommonStyle<QCommonStyle> {
	using Base = MyCommonStyle<QCommonStyle>;
public:
	using MyCommonStyle::Theme;
private:
	struct Private;
	Private *m;

	struct ScrollBarTextures {
		QImage page_bg;
		ButtonImages sub_line;
		ButtonImages add_line;
		ButtonImages slider;
	};

	QImage colorizeImage(QImage image);
	QImage loadColorizedImage(QString const &path, QString const &role = QString());
	ButtonImages generateButtonImages(QString const &path);
	QImage generateHoverImage(const QImage &source);
	QPixmap pixmapFromImage(QImage const &image, QSize size) const;
	void loadImages();

	QColor selectionColor() const
	{
		return QColor(72, 144, 255);
	}
	std::pair<QColor, QColor> menuBorderColors() const;

	void drawNinePatchImage(QPainter *p, QImage const &image, QRect const &r, int w, int h) const;
	void drawGutter(QPainter *p, QRect const &r) const;
	void drawSelectedItemFrame(QPainter *p, QRect rect, bool focus) const;
	void drawFocusFrame(QPainter *p, const QRect &rect, int margin) const;
	void drawButton(QPainter *p, QStyleOption const *option, bool mac_margin = true) const;
	void drawToolButton(QPainter *p, QStyleOption const *option) const;
	void drawMenuBarBG(QPainter *p, const QStyleOption *option, const QWidget *widget) const;
	QColor themeColor(int level, int alpha = 255) const;
	std::pair<QColor, QColor> buttonColor(State state) const;
	std::tuple<QColor, QColor> sliderKnobColor() const;
	void drawItemViewText(QPainter *p, const QStyleOptionViewItem *option, const QRect &rect, bool abbreviation) const;
	QRect indicatorRect(const QStyleOption *option, const QWidget *widget, const QRect &rect) const;
	qreal dpiScaled(qreal value, qreal dpi) const;
	qreal dpiScaled(qreal value, const QPaintDevice *device) const;
	qreal dpiScaled(qreal value, const QStyleOption *option) const;
public:
	DarkStyle(Theme theme, QColor theme_color = Qt::white);
	~DarkStyle() override;

	bool isDpiScalingEnabled() const;

	QColor themeColor() const;
	void setThemeColor(const QColor &color);
	void setScrollBarExtent(int n);
	void setDpiScalingEnabled(bool f);

	QPalette standardPalette() const override;
	QSize sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &size, const QWidget *widget) const override;
	void polish(QPalette &palette) override;
	int pixelMetric(PixelMetric metric, const QStyleOption *option = nullptr, const QWidget *widget = nullptr) const override;
	QRect subControlRect(ComplexControl cc, const QStyleOptionComplex *option, SubControl sc, const QWidget *widget) const override;
	int styleHint(StyleHint stylehint, const QStyleOption *option = nullptr, const QWidget *widget = nullptr, QStyleHintReturn *returnData = nullptr) const override;
	void drawItemText(QPainter *painter, const QRect &rect, int flags, const QPalette &pal, bool enabled, const QString &text, QPalette::ColorRole textRole = QPalette::NoRole) const override;
	void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const override;
	void drawControl(ControlElement ce, const QStyleOption *option, QPainter *p, const QWidget *widget) const override;
	void drawComplexControl(ComplexControl cc, const QStyleOptionComplex *option, QPainter *p, const QWidget *widget) const override;

};

#endif // DARKSTYLE_H
