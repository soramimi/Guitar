#ifndef DARKSTYLE_H
#define DARKSTYLE_H

#include <QPainter>
#include <QProxyStyle>

class DarkStyle : public QProxyStyle {
public:
private:
	struct Private;
	Private *m;

	class ButtonImages {
	public:
		QImage im_normal;
		QImage im_hover;
	};

	struct ScrollBarTextures {
		QImage page_bg;
		ButtonImages sub_line;
		ButtonImages add_line;
		ButtonImages slider;
	};

	QImage colorizeImage(QImage image);
	QImage loadColorizedImage(const QString &path, const QString &role = QString());
	ButtonImages generateButtonImages(const QString &path);
	QImage generateHoverImage(const QImage &source);
	QPixmap pixmapFromImage(QImage const &image, QSize size) const;
	void loadImages();

	QColor colorForSelectedFrame(QStyleOption const *opt) const;
	QColor colorForItemView(QStyleOption const *opt) const;

	void drawNinePatchImage(QPainter *p, QImage const &image, QRect const &r, int w, int h) const;
	void drawGutter(QPainter *p, QRect const &r) const;
	void drawSelectedMenuFrame(const QStyleOption *option, QPainter *p, QRect rect, QWidget const *widget, bool deep) const;
	void drawButton(QPainter *p, QStyleOption const *option) const;
	void drawToolButton(QPainter *p, QStyleOption const *option) const;
	void drawMenuBarBG(QPainter *p, const QStyleOption *option, const QWidget *widget) const;
	QColor color(int level, int alpha = 255) const;
public:
	DarkStyle(const QColor &base_color = QColor());
	~DarkStyle();

	QColor getBaseColor();
	void setBaseColor(QColor color);
	void setScrollBarExtent(int n);

	void polish(QPalette &palette);
	int pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const;
	QRect subControlRect(ComplexControl cc, const QStyleOptionComplex *option, SubControl sc, const QWidget *widget) const;
	void drawPrimitive(PrimitiveElement pe, const QStyleOption *option, QPainter *p, const QWidget *widget) const;
	void drawControl(ControlElement ce, const QStyleOption *option, QPainter *p, const QWidget *widget) const;
	void drawComplexControl(ComplexControl cc, const QStyleOptionComplex *option, QPainter *p, const QWidget *widget) const;
};

#endif // DARKSTYLE_H
