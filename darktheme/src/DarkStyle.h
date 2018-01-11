#ifndef DARKSTYLE_H
#define DARKSTYLE_H

#include "LegacyWindowsStyleTreeControl.h"

#include <QPainter>
#include <QProxyStyle>

class DarkStyle : public QProxyStyle {
public:
	class ButtonImages {
	public:
		QImage im_normal;
		QImage im_hover;
	};
private:
	struct ScrollBarTextures {
		QImage page_bg;
		ButtonImages sub_line;
		ButtonImages add_line;
		ButtonImages slider;
	};

	ScrollBarTextures hsb;
	ScrollBarTextures vsb;

	QImage button_normal;
	QImage button_press;

	QPixmap progress_horz;
	QPixmap progress_vert;

	LegacyWindowsStyleTreeControl legacy_windows_;

	static const int TEXTURE_CACHE_SIZE = 100;

	struct TextureCacheItem {
		QString key;
		QPixmap pm;
	};

	static QString pixmapkey(QString const &name, QString const &role, QSize const &size);
	static ButtonImages generateButtonImages(const QString &path);
	QPixmap pixmapFromImage(QImage const &image, QSize size) const;

	QColor colorForSelectedFrame(QStyleOption const *opt) const;
	QColor colorForItemView(QStyleOption const *opt) const;

	void drawNinePatchImage(QPainter *p, QImage const &image, QRect const &r, int w, int h) const;
	void drawGutter(QPainter *p, QRect const &r) const;
	void drawSelectedMenuFrame(const QStyleOption *option, QPainter *p, QRect rect, QWidget const *widget, bool deep) const;
	void drawButton(QPainter *p, QStyleOption const *option) const;
	void drawToolButton(QPainter *p, QStyleOption const *option) const;
	static void drawRaisedFrame(QPainter *p, const QRect &rect, const QPalette &palette);
	void drawMenuBarBG(QPainter *p, const QStyleOption *option, const QWidget *widget) const;
public:
	DarkStyle();
	void polish(QPalette &palette);
public:
	int pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const;
	QRect subControlRect(ComplexControl cc, const QStyleOptionComplex *option, SubControl sc, const QWidget *widget) const;
	void drawPrimitive(PrimitiveElement pe, const QStyleOption *option, QPainter *p, const QWidget *widget) const;
	void drawControl(ControlElement ce, const QStyleOption *option, QPainter *p, const QWidget *widget) const;
	void drawComplexControl(ComplexControl cc, const QStyleOptionComplex *option, QPainter *p, const QWidget *widget) const;
};

#endif // DARKSTYLE_H
