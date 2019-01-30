#include "DarkStyle.h"
#include "NinePatch.h"
#include "TraditionalWindowsStyleTreeControl.h"
#include <QApplication>
#include <QComboBox>
#include <QDebug>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QInputDialog>
#include <QListView>
#include <QMessageBox>
#include <QPixmapCache>
#include <QStyleOptionComplex>
#include <QStyleOptionFrameV3>
#include <QTableWidget>
#include <QToolTip>
#include <cmath>
#include <cstdint>

#define MBI_NORMAL                  1
#define MBI_HOT                     2
#define MBI_PUSHED                  3
#define MBI_DISABLED                4

namespace {

const int windowsItemFrame        =  2; // menu item frame width
const int windowsItemHMargin      =  3; // menu item hor text margin
const int windowsItemVMargin      =  4; // menu item ver text margin
const int windowsArrowHMargin     =  6; // arrow horizontal margin
const int windowsRightBorder      = 15; // right border on windows

void drawFrame(QPainter *pr, int x, int y, int w, int h, QColor color_topleft, QColor color_bottomright)
{
	if (w < 3 || h < 3) {
		if (w > 0 && h > 0) {
			pr->fillRect(x, y, w, h, color_topleft);
		}
	} else {
		if (!color_topleft.isValid()) color_topleft = Qt::black;
		if (!color_bottomright.isValid()) color_bottomright = color_topleft;
		pr->fillRect(x, y, w - 1, 1, color_topleft);
		pr->fillRect(x, y + 1, 1, h -1, color_topleft);
		pr->fillRect(x + w - 1, y, 1, h -1, color_bottomright);
		pr->fillRect(x + 1, y + h - 1, w - 1, 1, color_bottomright);
	}
}

void drawFrame(QPainter *pr, QRect const &r, QColor const &color_topleft, QColor const &color_bottomright)
{
	return drawFrame(pr, r.x(), r.y(), r.width(), r.height(), color_topleft, color_bottomright);
}

struct TextureCacheItem {
    QString key;
    QPixmap pm;
};

QString pixmapkey(QString const &name, QString const &role, QSize const &size, QColor const &color)
{
	QString key = "%1:%2:%3:%4:%5";
	key = key.arg(name).arg(role).arg(size.width()).arg(size.height()).arg(QString().sprintf("%02X%02X%02X", color.red(), color.green(), color.blue()));
    return key;
}

void drawRaisedFrame(QPainter *p, const QRect &rect, const QPalette &palette)
{
	p->save();
    int x = rect.x();
    int y = rect.y();
    int w = rect.width();
    int h = rect.height();
    p->setClipRect(x, y, w, h);
    p->fillRect(x, y, w, h, palette.color(QPalette::Window));
    p->fillRect(x, y, w - 1, 1, palette.color(QPalette::Light));
    p->fillRect(x, y, 1, h - 1, palette.color(QPalette::Light));
//    p->fillRect(x + 1, y + h - 2, w - 2, 1, palette.color(QPalette::Dark));
//    p->fillRect(x + w - 2, y + 1, 1, h - 2, palette.color(QPalette::Dark));
    p->fillRect(x, y + h - 1, w, 1, palette.color(QPalette::Shadow));
    p->fillRect(x + w - 1, y, 1, h, palette.color(QPalette::Shadow));
    p->restore();
}

QImage loadImage(QString const &path, QString const &role = QString())
{
	QImage image(path);
	image.setText("name", path);
	image.setText("role", role);
	return image;
}

QRgb colorize(QRgb color, int light, int alpha)
{
	int r, g, b;
	r = g = b = 0;
	if (alpha != 0) {
		r = qRed(color);
		g = qGreen(color);
		b = qBlue(color);
		if (light < 128) {
			r = r * light / 127;
			g = g * light / 127;
			b = b * light / 127;
		} else {
			int u = 255 - light;
			r = 255 - (255 - r) * u / 127;
			g = 255 - (255 - g) * u / 127;
			b = 255 - (255 - b) * u / 127;
		}
	}
	return qRgba(r, g, b, alpha);
}

} // namespace

// DarkStyle

static const int TEXTURE_CACHE_SIZE = 100;

struct DarkStyle::Private {
	QColor base_color;
	bool images_loaded = false;

    ScrollBarTextures hsb;
    ScrollBarTextures vsb;

    int scroll_bar_extent = -1;

    QImage button_normal;
    QImage button_press;

	QImage progress_horz;
	QImage progress_vert;

	TraditionalWindowsStyleTreeControl legacy_windows;

};

DarkStyle::DarkStyle(QColor const &base_color)
    : m(new Private)
{
	setBaseColor(base_color);
}

DarkStyle::~DarkStyle()
{
	delete m;
}

QColor DarkStyle::getBaseColor()
{
	return m->base_color;
}

void DarkStyle::setBaseColor(QColor const &color)
{
	m->base_color = color;
	m->images_loaded = false;
}

QColor DarkStyle::color(int level, int alpha) const
{
	QColor c = m->base_color.lighter(level * 100 / 255);
	c.setAlpha(alpha);
	return c;
}

void DarkStyle::setScrollBarExtent(int n)
{
    m->scroll_bar_extent = n;
}

QImage DarkStyle::colorizeImage(QImage image)
{
	int w = image.width();
	int h = image.height();
	if (w > 0 && h > 0) {
		QColor c = color(128);
		QRgb rgb = c.rgb();
		Q_ASSERT(image.format() == QImage::Format_ARGB32);
		for (int y = 0; y < h; y++) {
			QRgb *p = reinterpret_cast<QRgb *>(image.scanLine(y));
			for (int x = 0; x < w; x++) {
				p[x] = colorize(rgb, qGray(p[x]), qAlpha(p[x]));
			}
		}
	}
	return image;
}

QImage DarkStyle::loadColorizedImage(QString const &path, QString const &role)
{
	QImage image = loadImage(path);
	image = colorizeImage(image);
	image.setText("name", path);
	image.setText("role", role);
	return image;
}

namespace {
class Lighten {
private:
	int lut[256];
public:
	Lighten()
	{
#if 0
		const double x = 0.75;
		for (int i = 0; i < 256; i++) {
			lut[i] = (int)(pow(i / 255.0, x) * 255);
		}
#else
		for (int i = 0; i < 256; i++) {
			lut[i] = 255 - (255 - i) * 192 / 256;
		}
#endif
	}
	int operator [] (int i)
	{
		return lut[i];
	}
};
}

DarkStyle::ButtonImages DarkStyle::generateButtonImages(QString const &path)
{
	QImage source = loadImage(path);
	ButtonImages buttons;
	int w = source.width();
	int h = source.height();
	if (w > 4 && h > 4) {
		QColor c = color(128);
		QRgb rgb = c.rgb();
		Lighten lighten;
		buttons.im_normal = source;
		buttons.im_hover = source;
		w -= 4;
		h -= 4;
		for (int y = 0; y < h; y++) {
			QRgb *src1 = reinterpret_cast<QRgb *>(source.scanLine(y + 1));
			QRgb *src2 = reinterpret_cast<QRgb *>(source.scanLine(y + 2));
			QRgb *src3 = reinterpret_cast<QRgb *>(source.scanLine(y + 3));
			QRgb *dst0 = reinterpret_cast<QRgb *>(buttons.im_normal.scanLine(y + 2)) + 2;
			QRgb *dst1 = reinterpret_cast<QRgb *>(buttons.im_hover.scanLine(y + 2)) + 2;
			for (int x = 0; x < w; x++) {
				int v = (int)qAlpha(src3[x + 3]) - (int)qAlpha(src1[x + 1]);
				v = (v + 256) / 2;
				int alpha = qAlpha(src2[x + 2]);
				dst0[x] = colorize(rgb, v, alpha);
				v = lighten[v];
				dst1[x] = colorize(rgb, v, alpha);
			}
		}
		buttons.im_normal.setText("name", source.text("name"));
		buttons.im_hover.setText("name", source.text("name"));
		buttons.im_normal.setText("role", "normal");
		buttons.im_hover.setText("role", "hover");
	}
	return buttons;
}

QImage DarkStyle::generateHoverImage(QImage const &source)
{
	QImage newimage;
	int w = source.width();
	int h = source.height();
	if (w > 2 && h > 2) {
		QColor c = color(128);
		QRgb rgb = c.rgb();
		Lighten lighten;
		newimage = source;
		Q_ASSERT(newimage.format() == QImage::Format_ARGB32);
		w -= 2;
		h -= 2;
		for (int y = 0; y < h; y++) {
			QRgb *ptr = reinterpret_cast<QRgb *>(newimage.scanLine(y + 1)) + 1;
			for (int x = 0; x < w; x++) {
				int v = qGray(ptr[x]);
				v = lighten[v];
				ptr[x] = colorize(rgb, v, qAlpha(ptr[x]));
			}
		}
		newimage.setText("role", "hover");
	}
	return newimage;
}

void DarkStyle::loadImages()
{
	if (m->images_loaded) return;

	if (!m->base_color.isValid()) {
		setBaseColor(Qt::white);
	}

	m->button_normal        = loadColorizedImage(QLatin1String(":/darktheme/button/button_normal.png"), QLatin1String("normal"));
	m->button_press         = loadColorizedImage(QLatin1String(":/darktheme/button/button_press.png"), QLatin1String("press"));

	m->hsb.sub_line         = generateButtonImages(QLatin1String(":/darktheme/hsb/hsb_sub_line.png"));
	m->hsb.add_line         = generateButtonImages(QLatin1String(":/darktheme/hsb/hsb_add_line.png"));
	m->hsb.page_bg          = loadColorizedImage(QLatin1String(":/darktheme/hsb/hsb_page_bg.png"));
	m->hsb.slider.im_normal = loadColorizedImage(QLatin1String(":/darktheme/hsb/hsb_slider.png"));
	m->hsb.slider.im_hover  = generateHoverImage(m->hsb.slider.im_normal);

	m->vsb.sub_line         = generateButtonImages(QLatin1String(":/darktheme/vsb/vsb_sub_line.png"));
	m->vsb.add_line         = generateButtonImages(QLatin1String(":/darktheme/vsb/vsb_add_line.png"));
	m->vsb.page_bg          = loadColorizedImage(QLatin1String(":/darktheme/vsb/vsb_page_bg.png"));
	m->vsb.slider.im_normal = loadColorizedImage(QLatin1String(":/darktheme/vsb/vsb_slider.png"));
	m->vsb.slider.im_hover  = generateHoverImage(m->vsb.slider.im_normal);

	m->progress_horz = loadImage(QLatin1String(":/darktheme/progress/horz.png"));
	m->progress_vert = loadImage(QLatin1String(":/darktheme/progress/vert.png"));

	m->images_loaded = true;
}

QPixmap DarkStyle::pixmapFromImage(const QImage &image, QSize size) const
{
	QString key = pixmapkey(image.text("name"), image.text("role"), size, m->base_color);

	QPixmap *pm = QPixmapCache::find(key);
	if (pm) return *pm;

	TextureCacheItem t;
	t.key = key;
	t.pm = QPixmap::fromImage(image.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
	QPixmapCache::insert(key, t.pm);
	return t.pm;
}

QColor DarkStyle::selectionColor() const
{
//	return QColor(128, 192, 255);
	return QColor(80, 160, 255);
}

QColor DarkStyle::colorForItemView(QStyleOption const *opt) const
{
	return opt->palette.color(QPalette::Dark);
//	return opt->palette.color(QPalette::Base); // これじゃない
}

void DarkStyle::drawNinePatchImage(QPainter *p, const QImage &image, const QRect &r, int w, int h) const
{
	QImage im = createImageFromNinePatchImage(image, w, h);
	im.setText("name", image.text("name"));
	im.setText("role", image.text("role"));
	QPixmap pm = pixmapFromImage(im, r.size());
	p->drawPixmap(r.x(), r.y(), pm);
}

void DarkStyle::polish(QPalette &palette)
{
	if (!m->base_color.isValid()) {
		setBaseColor(Qt::white);
	}
	loadImages();
	palette = QPalette(color(64));
#ifndef Q_OS_WIN
	palette.setColor(QPalette::ToolTipText, Qt::black); // ツールチップの文字色
#endif
}

void DarkStyle::drawGutter(QPainter *p, const QRect &r) const
{
	int x = r.x();
	int y = r.y();
	int w = r.width();
	int h = r.height();
	QColor dark = color(32);
	QColor lite = color(128);
	if (w < h) {
		x += (w - 1) / 2;
		p->fillRect(x, y, 1, h, dark);
		p->fillRect(x + 1, y, 1, h, lite);
	} else if (w > h) {
		y += (h - 1) / 2;
		p->fillRect(x, y, w, 1, dark);
		p->fillRect(x, y + 1, w, 1, lite);
	}
}

void DarkStyle::drawSelectedItemFrame(QPainter *p, QRect rect, const QWidget *widget, bool deep) const
{
	(void)widget;
	QColor color = selectionColor();

	int x, y, w, h;
	x = rect.x();
	y = rect.y();
	w = rect.width();
	h = rect.height();

	auto SetAlpha = [&](QColor *color, int alpha){
		if (deep) {
			alpha = alpha * 3 / 2;
		}
		color->setAlpha(alpha);
	};

	QString key = QString::asprintf("selection_frame:%02x%02x%02x:%dx%d", color.red(), color.green(), color.blue(), w, h);

	QPixmap pixmap;
	if (!QPixmapCache::find(key, &pixmap)) {
		pixmap = QPixmap(w, h);
		pixmap.fill(Qt::transparent);
		QPainter pr(&pixmap);
		pr.setRenderHint(QPainter::Antialiasing);

		QColor pencolor = color;
		SetAlpha(&pencolor, 128);
		pr.setPen(pencolor);
		pr.setBrush(Qt::NoBrush);

		QPainterPath path;
		path.addRoundedRect(1.5, 1.5, w - 1.5, h - 1.5, 3, 3);

		pr.drawPath(path);

		pr.setClipPath(path);

		int a = color.alpha();
		QColor color0 = color;
		QColor color1 = color;
		SetAlpha(&color0, 96 * a / 255);
		SetAlpha(&color1, 32 * a / 255);
		QLinearGradient gr(QPointF(0, 0), QPointF(0, h));
		gr.setColorAt(0, color0);
		gr.setColorAt(1, color1);
		QBrush br(gr);
		pr.fillRect(0, 0, w, h, br);

		pr.end();
		QPixmapCache::insert(key, pixmap);
	}

	p->drawPixmap(x, y, w, h, pixmap);
}

void DarkStyle::drawSelectionFrame(QPainter *p, QRect const &rect, double margin) const
{
	QColor color = selectionColor();
	p->setPen(color);
	p->setBrush(Qt::NoBrush);
	p->drawRoundedRect(((QRectF)rect).adjusted(margin, margin, -margin, -margin), 4, 4);
}

void DarkStyle::drawButton(QPainter *p, const QStyleOption *option, bool mac_margin) const
{
	QRect rect = option->rect;
	int w =	rect.width();
	int h = rect.height();
	
#ifdef Q_OS_MAC
	if (mac_margin) {
		int margin = pixelMetric(PM_ButtonMargin, option, nullptr);
		if (margin > 0) {
			int n = std::min(w, h);
			if (n > margin * 2) {
				n = (n - margin * 2) / 2;
				if (n > margin) n = margin;
				rect = rect.adjusted(n, n, -n, -n);
				w = rect.width();
				h = rect.height();
			}
		}
	}
#else
	(void)mac_margin;
#endif
	
	bool pressed = (option->state & (State_Sunken | State_On));
	bool hover = (option->state & State_MouseOver);
	
	if (pressed) {
		drawNinePatchImage(p, m->button_press, rect, w, h);
	} else {
		drawNinePatchImage(p, m->button_normal, rect, w, h);
	}
	{
		QPainterPath path;
		path.addRoundedRect(rect, 6, 6); // 角丸四角形のパス
		p->save();
		p->setRenderHint(QPainter::Antialiasing);
		p->setClipPath(path);
		
		int x = rect.x();
		int y = rect.y();
		int w = rect.width();
		int h = rect.height();
		QColor color0, color1;
#ifdef Q_OS_MAC
		if (pressed) {
			color0 = Qt::black;
			color1 = Qt::black;
			color0.setAlpha(48);
			color1.setAlpha(144);
		} else {
			color0 = Qt::black;
			color1 = Qt::black;
			color0.setAlpha(32);
			color1.setAlpha(128);
		}
#else
		if (pressed) {
			color0 = Qt::black;
			color1 = Qt::black;
			color0.setAlpha(32);
			color1.setAlpha(128);
		} else if (hover) {
			color0 = Qt::black;
			color1 = Qt::black;
			color0.setAlpha(16);
			color1.setAlpha(96);
		} else {
			color0 = Qt::black;
			color1 = Qt::black;
			color0.setAlpha(32);
			color1.setAlpha(128);
		}
#endif
		QLinearGradient gr(QPointF(x, y), QPointF(x, y + h));
		gr.setColorAt(0, color0);
		gr.setColorAt(1, color1);
		QBrush br(gr);
		p->fillRect(x, y, w, h, br);
		
		if (option->state & State_HasFocus) {
#if 1
			drawSelectionFrame(p, rect, 3.5);
#else
			p->fillRect(x, y, w, h, QColor(80, 160, 255, 32));
#endif
		}
		
		p->restore();
	}
}

void DarkStyle::drawToolButton(QPainter *p, const QStyleOption *option) const
{
	p->save();

	bool hover = (option->state & State_MouseOver);
	bool pressed = (option->state & State_Sunken);
#ifdef Q_OS_MAC
	hover = false;
#endif

	int x = option->rect.x();
	int y = option->rect.y();
	int w = option->rect.width();
	int h = option->rect.height();
	QColor color0, color1;
#ifdef Q_OS_MAC
	if (pressed) {
		color0 = color(72);
		color1 = color(40);
	} else {
		color0 = color(80);
		color1 = color(48);
	}
#else
	if (pressed) {
		color0 = color(80);
		color1 = color(48);
	} else if (hover) {
		color0 = color(96);
		color1 = color(64);
	} else {
		color0 = color(80);
		color1 = color(48);
	}
#endif
	QLinearGradient gr(QPointF(x, y), QPointF(x, y + h));
	gr.setColorAt(0, color0);
	gr.setColorAt(1, color1);
	QBrush br(gr);
	p->fillRect(x, y, w, h, br);

	if (option->state & State_Raised) {
		drawFrame(p, option->rect, color(96), color(32));
	} else if (option->state & State_Sunken) {
		drawFrame(p, option->rect, color(48), color(48));
	}

	p->restore();
}

void DarkStyle::drawMenuBarBG(QPainter *p, const QStyleOption *option, QWidget const *widget) const
{
	int x = option->rect.x();
	int y = widget->y();
	int w = option->rect.width();
	int h = widget->height();
	QLinearGradient gradient;
	gradient.setStart(x, y);
	gradient.setFinalStop(x, y + h / 2.0);
	gradient.setColorAt(0, option->palette.color(QPalette::Light));
	gradient.setColorAt(1, option->palette.color(QPalette::Window));
	p->fillRect(x, y, w, h, gradient);
	p->fillRect(x, y + h - 1, w, 1, option->palette.color(QPalette::Dark));
}

int DarkStyle::pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const
{
    if (metric == PM_ScrollBarExtent) {
        if (m->scroll_bar_extent > 0) {
            return m->scroll_bar_extent;
        }
    }
	if (metric == PM_SliderLength) {
		return std::min(widget->width(), widget->height());
	}
	return QProxyStyle::pixelMetric(metric, option, widget);
}

QRect DarkStyle::subControlRect(ComplexControl cc, const QStyleOptionComplex *option, SubControl sc, const QWidget *widget) const
{
	if (cc == CC_Slider && sc == SC_SliderGroove) {
		return widget->rect();
	}
	if (cc == CC_Slider && sc == SC_SliderHandle) {
		if (auto const *o = qstyleoption_cast<QStyleOptionSlider const *>(option)) {
			QRect rect;
			int extent = std::min(widget->width(), widget->height());
			int span = pixelMetric(PM_SliderLength, o, widget);
			bool horizontal = o->orientation == Qt::Horizontal;
			int sliderPos = sliderPositionFromValue(o->minimum, o->maximum,
													o->sliderPosition,
													(horizontal ? o->rect.width()
																: o->rect.height()) - span,
													o->upsideDown);
			if (horizontal) {
				rect.setRect(o->rect.x() + sliderPos, o->rect.y(), span, extent);
			} else {
				rect.setRect(o->rect.x(), o->rect.y() + sliderPos, extent, span);
			}
			return rect;
		}
	}
	if (cc == CC_GroupBox) {
		QRect ret;
		if (auto const *o = qstyleoption_cast<QStyleOptionGroupBox const *>(option)) {
			switch (sc) {
			case SC_GroupBoxFrame:
			case SC_GroupBoxContents:
				{
					int topMargin = 0;
					int topHeight = 0;
					int verticalAlignment = proxy()->styleHint(SH_GroupBox_TextLabelVerticalAlignment, o, widget);
#ifdef Q_OS_MACX
					verticalAlignment |= Qt::AlignVCenter;
#endif
					if (o->text.size() || (o->subControls & QStyle::SC_GroupBoxCheckBox)) {
						topHeight = o->fontMetrics.height();
						if (verticalAlignment & Qt::AlignVCenter) {
							topMargin = topHeight / 2;
						} else if (verticalAlignment & Qt::AlignTop) {
							topMargin = topHeight;
						}
					}

					QRect frameRect = o->rect;
					frameRect.setTop(topMargin);

					if (sc == SC_GroupBoxFrame) {
						ret = frameRect;
						break;
					}

					int frameWidth = 0;
					if ((o->features & QStyleOptionFrame::Flat) == 0) {
						frameWidth = proxy()->pixelMetric(PM_DefaultFrameWidth, o, widget);
					}
					ret = frameRect.adjusted(frameWidth, frameWidth + topHeight - topMargin, -frameWidth, -frameWidth);
				}
				break;
			case SC_GroupBoxCheckBox:
			case SC_GroupBoxLabel:
				{
					QFontMetrics fontMetrics = o->fontMetrics;
					int h = fontMetrics.height();
					int tw = fontMetrics.size(Qt::TextShowMnemonic, o->text + QLatin1Char(' ')).width();
					int marg = (o->features & QStyleOptionFrame::Flat) ? 0 : 8;
					ret = o->rect.adjusted(marg, 0, -marg, 0);
					ret.setHeight(h);

					int indicatorWidth = proxy()->pixelMetric(PM_IndicatorWidth, option, widget);
					int indicatorSpace = proxy()->pixelMetric(PM_CheckBoxLabelSpacing, option, widget) - 1;
					bool hasCheckBox = o->subControls & QStyle::SC_GroupBoxCheckBox;
					int checkBoxSize = hasCheckBox ? (indicatorWidth + indicatorSpace) : 0;

					// Adjusted rect for label + indicatorWidth + indicatorSpace
					QRect totalRect = alignedRect(o->direction, o->textAlignment, QSize(tw + checkBoxSize, h), ret);

					// Adjust totalRect if checkbox is set
					if (hasCheckBox) {
						bool ltr = o->direction == Qt::LeftToRight;
						int left = 0;
						// Adjust for check box
						if (sc == SC_GroupBoxCheckBox) {
							int indicatorHeight = proxy()->pixelMetric(PM_IndicatorHeight, option, widget);
							left = ltr ? totalRect.left() : (totalRect.right() - indicatorWidth);
							int top = totalRect.top() + qMax(0, fontMetrics.height() - indicatorHeight) / 2;
							totalRect.setRect(left, top, indicatorWidth, indicatorHeight);
							// Adjust for label
						} else {
							left = ltr ? (totalRect.left() + checkBoxSize - 2) : totalRect.left();
							totalRect.setRect(left, totalRect.top(), totalRect.width() - checkBoxSize, totalRect.height());
						}
					}
					ret = totalRect;
				}
				break;
			}
			return ret;
		}
	}
	return QProxyStyle::subControlRect(cc, option, sc, widget);
}

void DarkStyle::drawPrimitive(PrimitiveElement pe, const QStyleOption *option, QPainter *p, const QWidget *widget) const
{
//    qDebug() << pe;
#ifdef Q_OS_LINUX
	if (pe == PE_FrameFocusRect) {
		QColor color(64, 128, 255);
		drawFrame(p, option->rect, color, color);
		return;
	}
#endif
	if (pe == PE_IndicatorArrowDown) {
		switch (pe) {
		case PE_IndicatorArrowUp:
		case PE_IndicatorArrowDown:
		case PE_IndicatorArrowRight:
		case PE_IndicatorArrowLeft:
			{
				if (option->rect.width() <= 1 || option->rect.height() <= 1) {
					break;
				}
				QRect r = option->rect.adjusted(-10, -10, 0, 10);
				int size = qMin(r.height(), r.width());
				QPixmap pixmap;
				if (1) {
					int border = size / 5;
					int sqsize = 2 * (size / 2);
					QImage image(sqsize, sqsize, QImage::Format_ARGB32_Premultiplied);
					image.fill(0);
					QPainter imagePainter(&image);

					QPolygon a;
					switch (pe) {
					case PE_IndicatorArrowUp:
						a.setPoints(3, border, sqsize / 2,  sqsize / 2, border,  sqsize - border, sqsize / 2);
						break;
					case PE_IndicatorArrowDown:
						a.setPoints(3, border, sqsize / 2,  sqsize / 2, sqsize - border,  sqsize - border, sqsize / 2);
						break;
					case PE_IndicatorArrowRight:
						a.setPoints(3, sqsize - border, sqsize / 2,  sqsize / 2, border,  sqsize / 2, sqsize - border);
						break;
					case PE_IndicatorArrowLeft:
						a.setPoints(3, border, sqsize / 2,  sqsize / 2, border,  sqsize / 2, sqsize - border);
						break;
					default:
						break;
					}

					int bsx = 0;
					int bsy = 0;

					if (option->state & State_Sunken) {
						bsx = pixelMetric(PM_ButtonShiftHorizontal, option, widget);
						bsy = pixelMetric(PM_ButtonShiftVertical, option, widget);
					}

					QRect bounds = a.boundingRect();
					int sx = sqsize / 2 - bounds.center().x() - 1;
					int sy = sqsize / 2 - bounds.center().y() - 1;
					imagePainter.translate(sx + bsx, sy + bsy);
					imagePainter.setPen(option->palette.buttonText().color());
					imagePainter.setBrush(option->palette.buttonText());
					imagePainter.setRenderHint(QPainter::Qt4CompatiblePainting);

					if (!(option->state & State_Enabled)) {
						imagePainter.translate(1, 1);
						imagePainter.setBrush(option->palette.light().color());
						imagePainter.setPen(option->palette.light().color());
						imagePainter.drawPolygon(a);
						imagePainter.translate(-1, -1);
						imagePainter.setBrush(option->palette.mid().color());
						imagePainter.setPen(option->palette.mid().color());
					}

					imagePainter.drawPolygon(a);
					imagePainter.end();
					pixmap = QPixmap::fromImage(image);
				}
				int xOffset = r.x() + (r.width() - size)/2;
				int yOffset = r.y() + (r.height() - size)/2;
				p->drawPixmap(xOffset, yOffset, pixmap);
			}
		}
		return;
	}
	if (pe == PE_PanelMenu) {
		QRect r = option->rect;
		drawFrame(p, r, Qt::black, Qt::black);
		r = r.adjusted(1, 1, -1, -1);
		drawFrame(p, r, color(128), color(64));
		r = r.adjusted(1, 1, -1, -1);
		p->fillRect(r, color(80));
		return;
	}
	if (pe == PE_FrameMenu) {
		return;
	}
	if (pe == PE_PanelButtonBevel) {
		drawButton(p, option);
		return;
	}
	if (pe == PE_PanelStatusBar) {
		p->fillRect(option->rect, option->palette.color(QPalette::Window));
		return;
	}
	if (pe == PE_FrameTabWidget) {
		if (auto const *o = qstyleoption_cast<QStyleOptionTabWidgetFrame const *>(option)) {
			int x = o->rect.x();
			int y = o->rect.y();
			int w = o->rect.width();
			int h = o->rect.height();
#ifdef Q_OS_WIN
			switch (o->shape) {
			case QTabBar::RoundedNorth:
				break;
			case QTabBar::RoundedSouth:
				h -= 1;
				break;
			case QTabBar::RoundedWest:
				break;
			case QTabBar::RoundedEast:
				w -= 2;
				break;
			}
#endif
			drawRaisedFrame(p, QRect(x, y, w, h), o->palette);
			return;
		}
	}
	if (pe == PE_PanelLineEdit) {
		if (auto const *panel = qstyleoption_cast<QStyleOptionFrame const *>(option)) {
			p->fillRect(option->rect, colorForItemView(option));
			if (panel->lineWidth > 0) {
				drawFrame(p, option->rect, option->palette.color(QPalette::Shadow), option->palette.color(QPalette::Light));
			}
		}
		return;
	}
	if (pe == PE_FrameGroupBox) {
		p->save();
		p->setRenderHint(QPainter::Antialiasing);
		QRectF r = option->rect;
        r = r.adjusted(1.5, 1.5, -0.5, -0.5);
		p->setPen(option->palette.color(QPalette::Light));
		p->drawRoundedRect(r, 5, 5);
		r = r.adjusted(-1, -1, -1, -1);
		p->setPen(option->palette.color(QPalette::Dark));
		p->drawRoundedRect(r, 5, 5);
		p->restore();
		return;
	}
	if (pe == PE_PanelItemViewRow) {
#ifdef Q_OS_WIN
		// thru
#else
		return;
#endif
	}
	if (pe == PE_PanelItemViewItem) {
//		p->fillRect(option->rect, colorForItemView(option)); // 選択枠を透過描画させるので背景は描かない
		if (auto const *tableview = qobject_cast<QTableView const *>(widget)) {
			Qt::PenStyle grid_pen_style = Qt::NoPen;
			if (tableview->showGrid()) {
				grid_pen_style = tableview->gridStyle();
			}
			QAbstractItemView::SelectionBehavior selection_behavior = tableview->selectionBehavior();
			if (grid_pen_style != Qt::NoPen) {
				int x = option->rect.x();
				int y = option->rect.y();
				int w = option->rect.width();
				int h = option->rect.height();
				p->fillRect(x + w - 1, y, 1, h, option->palette.color(QPalette::Dark));
				p->fillRect(x, y + h - 1, w, 1, option->palette.color(QPalette::Dark));
			}
			if (option->state & State_Selected) {
				p->save();
				p->setClipRect(option->rect);
				QRect r = widget->rect();
				if (selection_behavior == QAbstractItemView::SelectionBehavior::SelectRows) {
					r = QRect(r.x(), option->rect.y(), r.width(), option->rect.height());
				} else if (selection_behavior == QAbstractItemView::SelectionBehavior::SelectRows) {
					r = QRect(option->rect.x(), r.y(), option->rect.y(), r.height());
				}
				drawSelectedItemFrame(p, r, widget);
				p->restore();
			}
		} else {
			int n = 0;
			if (option->state & State_Selected) {
				n++;
			}
			if (n > 0) {
				drawSelectedItemFrame(p, option->rect, widget, n > 1);
			}
		}
		return;
	}
	if (pe == QStyle::PE_IndicatorBranch) {
		QColor bg = option->palette.color(QPalette::Base);
		p->fillRect(option->rect, bg);

        if (m->legacy_windows.drawPrimitive(pe, option, p, widget)) {
			return;
		}
	}
	if (pe == QStyle::PE_Widget) { // bg for messagebox
		const QDialogButtonBox *buttonBox = nullptr;

		if (qobject_cast<const QMessageBox *> (widget))
			buttonBox = widget->findChild<const QDialogButtonBox *>(QLatin1String("qt_msgbox_buttonbox"));
#ifndef QT_NO_INPUTDIALOG
		else if (qobject_cast<const QInputDialog *> (widget))
			buttonBox = widget->findChild<const QDialogButtonBox *>(QLatin1String("qt_inputdlg_buttonbox"));
#endif // QT_NO_INPUTDIALOG

		if (buttonBox) {
			int y = buttonBox->geometry().top();
			QRect r(option->rect.x(), y, option->rect.width(), 1);
			p->fillRect(r, option->palette.color(QPalette::Light));
			r.translate(0, -1);
			p->fillRect(r, option->palette.color(QPalette::Dark));
		}
		return;
	}
#ifndef Q_OS_WIN
	if (pe == QStyle::PE_PanelTipLabel) {
		// ツールチップの背景パネル
		p->fillRect(option->rect, QColor(255, 255, 192));
		drawFrame(p, option->rect, Qt::black, Qt::black);
		return;
	}
#endif
//	qDebug() << pe;
	QProxyStyle::drawPrimitive(pe, option, p, widget);
}

void DarkStyle::drawControl(ControlElement ce, const QStyleOption *option, QPainter *p, const QWidget *widget) const
{
//    qDebug() << ce;
	bool disabled = !(option->state & State_Enabled);
#ifdef Q_OS_MAC
    if (ce == CE_ToolBar) {
		int x = option->rect.x();
		int y = option->rect.y();
		int w = option->rect.width();
		int h = option->rect.height();
		p->fillRect(x, y + h - 1, w, 1, color(32));
		return;
	}
#endif
#ifdef Q_OS_LINUX
	if (ce == CE_ToolBar) {
		int x = option->rect.x();
		int y = option->rect.y();
		int w = option->rect.width();
		int h = option->rect.height();
		QColor color = option->palette.color(QPalette::Window);
		p->fillRect(x, y + h - 1, w, 1, color);
		return;
	}
	if (ce == CE_PushButtonLabel) {
		 if (auto const *o = qstyleoption_cast<QStyleOptionButton const *>(option)) {
			 QRect ir = o->rect;
			 uint tf = Qt::AlignVCenter | Qt::TextShowMnemonic;
			 QPoint buttonShift;

			 if (option->state & State_Sunken) {
				 buttonShift = QPoint(pixelMetric(PM_ButtonShiftHorizontal, option, widget), proxy()->pixelMetric(PM_ButtonShiftVertical, option, widget));
			 }

			 if (proxy()->styleHint(SH_UnderlineShortcut, o, widget)) {
				 tf |= Qt::TextShowMnemonic;
			 } else {
				 tf |= Qt::TextHideMnemonic;
			 }

			 if (!o->icon.isNull()) {
				 //Center both icon and text
				 QPoint point;

				 QIcon::Mode mode = o->state & State_Enabled ? QIcon::Normal : QIcon::Disabled;
				 if (mode == QIcon::Normal && o->state & State_HasFocus) {
					 mode = QIcon::Active;
				 }

				 QIcon::State state = QIcon::Off;

				 if (o->state & State_On) {
					 state = QIcon::On;
				 }

				 QPixmap pixmap = o->icon.pixmap(o->iconSize, mode, state);
				 int w = pixmap.width();
				 int h = pixmap.height();

				 if (!o->text.isEmpty()) {
					 w += o->fontMetrics.boundingRect(option->rect, tf, o->text).width() + 4;
				 }

				 point = QPoint(ir.x() + ir.width() / 2 - w / 2, ir.y() + ir.height() / 2 - h / 2);

				 if (o->direction == Qt::RightToLeft) {
					 point.rx() += pixmap.width();
				 }

				 p->drawPixmap(visualPos(o->direction, o->rect, point + buttonShift), pixmap);

				 if (o->direction == Qt::RightToLeft) {
					 ir.translate(-point.x() - 2, 0);
				 } else {
					 ir.translate(point.x() + pixmap.width() + 2, 0);
				 }

				 // left-align text if there is
				 if (!o->text.isEmpty()) {
					 tf |= Qt::AlignLeft;
				 }

			 } else {
				 tf |= Qt::AlignHCenter;
			 }

			 ir.translate(buttonShift);

			 if (o->features & QStyleOptionButton::HasMenu) {
				 ir = ir.adjusted(0, 0, -pixelMetric(PM_MenuButtonIndicator, o, widget), 0);
			 }

			 drawItemText(p, ir, tf, option->palette, (o->state & State_Enabled), o->text, QPalette::ButtonText);
			 return;
		 }
	}
	if (ce == CE_RadioButton || ce == CE_CheckBox) {
		if (auto const *o = qstyleoption_cast<QStyleOptionButton const *>(option)) {
			bool isRadio = (ce == CE_RadioButton);

			QStyleOptionButton subopt = *o;
			subopt.rect = subElementRect(isRadio ? SE_RadioButtonIndicator : SE_CheckBoxIndicator, o, widget);
			proxy()->drawPrimitive(isRadio ? PE_IndicatorRadioButton : PE_IndicatorCheckBox, &subopt, p, widget);
			subopt.rect = subElementRect(isRadio ? SE_RadioButtonContents : SE_CheckBoxContents, o, widget);

			drawControl(isRadio ? CE_RadioButtonLabel : CE_CheckBoxLabel, &subopt, p, widget);

			if (o->state & State_HasFocus) {
				QStyleOptionFocusRect fropt;
				fropt.QStyleOption::operator=(*o);
				fropt.rect = subElementRect(isRadio ? SE_RadioButtonFocusRect : SE_CheckBoxFocusRect, o, widget);
				drawPrimitive(PE_FrameFocusRect, &fropt, p, widget);
			}
			return;
		}
	}
	if (ce == CE_RadioButtonLabel || ce == CE_CheckBoxLabel) {
		if (auto const *o = qstyleoption_cast<QStyleOptionButton const *>(option)) {
			uint alignment = visualAlignment(o->direction, Qt::AlignLeft | Qt::AlignVCenter);

			if (!styleHint(SH_UnderlineShortcut, o, widget)) {
				alignment |= Qt::TextHideMnemonic;
			}
			QPixmap pix;
			QRect textRect = o->rect;
			if (!o->icon.isNull()) {
				pix = o->icon.pixmap(o->iconSize, o->state & State_Enabled ? QIcon::Normal : QIcon::Disabled);
				drawItemPixmap(p, o->rect, alignment, pix);
				if (o->direction == Qt::RightToLeft) {
					textRect.setRight(textRect.right() - o->iconSize.width() - 4);
				} else {
					textRect.setLeft(textRect.left() + o->iconSize.width() + 4);
				}
			}
			if (!o->text.isEmpty()){
				drawItemText(p, textRect, alignment | Qt::TextShowMnemonic, o->palette, o->state & State_Enabled, o->text, QPalette::ButtonText);
			}
			return;
		}
	}
	if (ce == CE_ComboBoxLabel) {
		if (auto const *o = qstyleoption_cast<QStyleOptionComboBox const *>(option)) {
			QRect editRect = subControlRect(CC_ComboBox, o, SC_ComboBoxEditField, widget);

			uint alignment = Qt::AlignLeft | Qt::AlignVCenter;

			if (!styleHint(SH_UnderlineShortcut, o, widget)) {
				alignment |= Qt::TextHideMnemonic;
			}
			QPixmap pix;
			QRect iconRect(editRect);
			QIcon icon = o->currentIcon;
			QString text = o->currentText;
			if (!icon.isNull()) {
				pix = icon.pixmap(o->iconSize, o->state & State_Enabled ? QIcon::Normal : QIcon::Disabled);
				drawItemPixmap(p, iconRect, alignment, pix);
				if (o->direction == Qt::RightToLeft) {
					editRect.setRight(editRect.right() - o->iconSize.width() - 4);
				} else {
					editRect.setLeft(editRect.left() + o->iconSize.width() + 4);
				}
			}
			if (!text.isEmpty()){
				drawItemText(p, editRect, alignment, o->palette, o->state & State_Enabled, text, QPalette::ButtonText);
			}
			return;
		}
	}
#endif
	if (ce == CE_ShapedFrame) {
		if (auto const *o = qstyleoption_cast<QStyleOptionFrameV3 const *>(option)) {
			int lw = o->lineWidth;
			if (lw > 0) {
				QRect r = o->rect;
				if (o->frameShape == QFrame::Panel || o->frameShape == QFrame::HLine || o->frameShape == QFrame::VLine) {
					if (o->state & (State_Sunken | State_Raised)) {
						if (r.width() < r.height()) {
							if (r.width() > 2) {
								r.adjust(0, 0, -1, 0);
							}
						} else {
							if (r.height() > 2) {
								r.adjust(0, 0, 0, -1);
							}
						}
						int x = r.x();
						int y = r.y();
						int w = r.width();
						int h = r.height();
						QColor color_topleft     = o->palette.color(QPalette::Dark);
						QColor color_bottomright = o->palette.color(QPalette::Light);
						if (o->state & State_Raised) {
							std::swap(color_topleft, color_bottomright);
						}
						int t = lw;
						if (o->frameShape == QFrame::HLine) {
							y = h / 2 - t;
							h = t * 2;
						} else if (o->frameShape == QFrame::VLine) {
							x = w / 2 - t;
							w = t * 2;
						}
						p->fillRect(x, y, w - t, t, color_topleft);
						p->fillRect(x, y + t, t, h - t, color_topleft);
						p->fillRect(x + w - t, y, t, h - t, color_bottomright);
						p->fillRect(x + t, y + h - t, w - t, t, color_bottomright);
					} else {
						if (r.width() < r.height()) {
							if (lw < r.width()) {
								r = QRect(r.x() + (r.width() - lw) / 2, r.y(), lw, r.height());
							}
						} else {
							if (lw < r.height()) {
								r = QRect(r.x(), r.y() + (r.height() - lw) / 2, r.width(), lw);
							}
						}
						int x = r.x();
						int y = r.y();
						int w = r.width();
						int h = r.height();
						QColor color = o->palette.color(QPalette::Shadow);
						p->fillRect(x, y, w, h, color);
					}
				} else if (o->frameShape == QFrame::Box) {
					int x = r.x();
					int y = r.y();
					int w = r.width();
					int h = r.height();
					if (w > 1) w--;
					if (h > 1) h--;
					QColor color_topleft     = o->palette.color(QPalette::Dark);
					QColor color_bottomright = o->palette.color(QPalette::Light);
					if (o->state & State_Raised) {
						std::swap(color_topleft, color_bottomright);
					}
					drawFrame(p, x + 1, y + 1, w, h, color_bottomright, color_bottomright);
					drawFrame(p, x, y, w, h, color_topleft, color_topleft);
				} else if (o->frameShape == QFrame::StyledPanel) {
					QColor color = o->palette.color(QPalette::Midlight);
					int x = r.x();
					int y = r.y();
					int w = r.width();
					int h = r.height();
					drawFrame(p, x, y, w, h, color, color);
				}
			}
		}
		return;
	}
	if (ce == CE_PushButtonBevel) {
		if (auto const *o = qstyleoption_cast<QStyleOptionButton const *>(option)) {
			if (o->features & QStyleOptionButton::Flat) {
				// nop
			} else {
				drawButton(p, option);
			}

			if (o->features & QStyleOptionButton::HasMenu) {
				int mbiw = 0, mbih = 0;
				QRect r = subElementRect(SE_PushButtonContents, option, nullptr);
				QStyleOptionButton newBtn = *o;
				r = QRect(r.right() - mbiw - 2, option->rect.top() + (option->rect.height()/2) - (mbih/2), mbiw + 1, mbih + 1);
				newBtn.rect = QStyle::visualRect(option->direction, option->rect, r);
				drawPrimitive(PE_IndicatorArrowDown, &newBtn, p, widget);
			}
		}
		return;
	}
	if (ce == CE_MenuBarEmptyArea) {
		drawMenuBarBG(p, option, widget);
		return;
	}
	if (ce == CE_MenuBarItem) {
		drawMenuBarBG(p, option, widget);
		if (option->state & State_Selected) {
			drawSelectedItemFrame(p, option->rect, widget);
		}
		if (auto const *o = qstyleoption_cast<QStyleOptionMenuItem const *>(option)) {
			QPalette::ColorRole textRole = disabled ? QPalette::Text : QPalette::ButtonText;
			QPixmap pix = o->icon.pixmap(pixelMetric(PM_SmallIconSize, option, widget), QIcon::Normal);
			uint alignment = Qt::AlignCenter | Qt::TextShowMnemonic | Qt::TextDontClip | Qt::TextSingleLine;
			if (!styleHint(SH_UnderlineShortcut, o, widget)) {
				alignment |= Qt::TextHideMnemonic;
			}
			if (!pix.isNull()) {
				drawItemPixmap(p, o->rect, alignment, pix);
			} else {
				drawItemText(p, o->rect, alignment, o->palette, o->state & State_Enabled, o->text, textRole);
			}
		}
		return;
	}
	if (ce == CE_MenuEmptyArea) {
		return;
	}
	if (ce == CE_MenuItem) {
		if (auto const *o = qstyleoption_cast<QStyleOptionMenuItem const *>(option)) {
			// windows always has a check column, regardless whether we have an icon or not
			int checkcol = 25;// / QWindowsXPStylePrivate::devicePixelRatio(widget);
			const int gutterWidth = 3;// / QWindowsXPStylePrivate::devicePixelRatio(widget);
			QRect rect = option->rect;

			bool ignoreCheckMark = false;
			if (qobject_cast<const QComboBox*>(widget)) {
				{ // bg
					int x = option->rect.x();
					int y = option->rect.y();
					int w = option->rect.width();
					int h = option->rect.height();
					p->fillRect(x, y, w, h, option->palette.color(QPalette::Light));
				}
				ignoreCheckMark = true; //ignore the checkmarks provided by the QComboMenuDelegate
			}

			//draw vertical menu line
			if (option->direction == Qt::LeftToRight) {
				checkcol += rect.x();
			}

			int x, y, w, h;
			o->rect.getRect(&x, &y, &w, &h);
			int tab = o->tabWidth;
			bool dis = !(o->state & State_Enabled);
			bool checked = (o->checkType != QStyleOptionMenuItem::NotCheckable) && o->checked;
			bool act = o->state & State_Selected;

			if (o->menuItemType == QStyleOptionMenuItem::Separator) {
				QRect r = option->rect.adjusted(2, 0, -2, 0);
				drawGutter(p, r);
				return;
			}

			QRect vCheckRect = visualRect(option->direction, o->rect, QRect(o->rect.x(), o->rect.y(), checkcol - (gutterWidth + o->rect.x()), o->rect.height()));

			if (act) {
				drawSelectedItemFrame(p, option->rect, widget);
			}

			if (checked && !ignoreCheckMark) {
				const qreal boxMargin = 3.5;
				const qreal boxWidth = checkcol - 2 * boxMargin;
				const int checkColHOffset = windowsItemHMargin + windowsItemFrame - 1;
				QRectF checkRectF(option->rect.left() + boxMargin + checkColHOffset, option->rect.center().y() - boxWidth/2 + 1, boxWidth, boxWidth);
				QRect checkRect = checkRectF.toRect();
				checkRect.setWidth(checkRect.height()); // avoid .toRect() round error results in non-perfect square
				checkRect = visualRect(o->direction, o->rect, checkRect);

				QStyleOptionButton box;
				box.QStyleOption::operator=(*option);
				box.rect = checkRect;
				if (checked) {
					box.state |= State_On;
				}
				drawPrimitive(PE_IndicatorCheckBox, &box, p, widget);
			}

			if (!ignoreCheckMark) {
				if (!o->icon.isNull()) {
					QIcon::Mode mode = dis ? QIcon::Disabled : QIcon::Normal;
					if (act && !dis) {
						mode = QIcon::Active;
					}
					QPixmap pixmap;
					if (checked) {
						pixmap = o->icon.pixmap(pixelMetric(PM_SmallIconSize, option, widget), mode, QIcon::On);
					} else {
						pixmap = o->icon.pixmap(pixelMetric(PM_SmallIconSize, option, widget), mode);
					}
					double dpr = pixmap.devicePixelRatio();
					if (dpr > 0) {
						const int pixw = int(pixmap.width() / dpr);
						const int pixh = int(pixmap.height() / dpr);
						QRect pmr(0, 0, pixw, pixh);
						pmr.moveCenter(vCheckRect.center());
						p->setPen(o->palette.text().color());
						p->drawPixmap(pmr.topLeft(), pixmap);
					}
				}
			} else {
				if (o->icon.isNull()) {
					checkcol = 0;
				} else {
					checkcol = o->maxIconWidth;
				}
			}

			p->setPen(o->palette.buttonText().color());

			const QColor textColor = o->palette.text().color();
			if (dis) {
				p->setPen(textColor);
			}

			int xm = windowsItemFrame + checkcol + windowsItemHMargin + (gutterWidth - o->rect.x()) - 1;
			int xpos = o->rect.x() + xm;
			QRect textRect(xpos, y + windowsItemVMargin, w - xm - windowsRightBorder - tab + 1, h - 2 * windowsItemVMargin);
			QRect vTextRect = visualRect(option->direction, o->rect, textRect);
			QString s = o->text;
			if (!s.isEmpty()) {    // draw text
				p->save();
				int t = s.indexOf(QLatin1Char('\t'));
				int text_flags = Qt::AlignVCenter | Qt::TextShowMnemonic | Qt::TextDontClip | Qt::TextSingleLine;
				if (!styleHint(SH_UnderlineShortcut, o, widget)) {
					text_flags |= Qt::TextHideMnemonic;
				}
				text_flags |= Qt::AlignLeft;
				if (t >= 0) {
					QRect vShortcutRect = visualRect(option->direction, o->rect, QRect(textRect.topRight(), QPoint(o->rect.right(), textRect.bottom())));
					p->drawText(vShortcutRect, text_flags, s.mid(t + 1));
					s = s.left(t);
				}
				QFont font = o->font;
				if (o->menuItemType == QStyleOptionMenuItem::DefaultItem) {
					font.setBold(true);
				}
				p->setFont(font);
				p->setPen(textColor);
				p->drawText(vTextRect, text_flags, s.left(t));
				p->restore();
			}
			if (o->menuItemType == QStyleOptionMenuItem::SubMenu) {// draw sub menu arrow
				int dim = (h - 2 * windowsItemFrame) / 2;
				PrimitiveElement arrow;
				arrow = (option->direction == Qt::RightToLeft) ? PE_IndicatorArrowLeft : PE_IndicatorArrowRight;
				xpos = x + w - windowsArrowHMargin - windowsItemFrame - dim;
				QRect  vSubMenuRect = visualRect(option->direction, o->rect, QRect(xpos, y + h / 2 - dim / 2, dim, dim));
				QStyleOptionMenuItem newMI = *o;
				newMI.rect = vSubMenuRect;
				newMI.state = dis ? State_None : State_Enabled;
				drawPrimitive(arrow, &newMI, p, widget);
			}
		}
		return;
	}
	if (ce == CE_TabBarTabShape) {
#ifdef Q_OS_MAC
		if (auto const *o = qstyleoption_cast<QStyleOptionTab const *>(option)) {
			drawButton(p, option, false);
			bool selected = o->state & State_Selected;
			if (selected) {
				drawSelectedItemFrame(p, o->rect.adjusted(0, 0, -2, -2), widget);
			}
			return;
		}
#else
		if (auto const *o = qstyleoption_cast<QStyleOptionTab const *>(option)) {
			bool rtlHorTabs = (o->direction == Qt::RightToLeft && (o->shape == QTabBar::RoundedNorth || o->shape == QTabBar::RoundedSouth));
			bool selected = o->state & State_Selected;
			bool lastTab = ((!rtlHorTabs && o->position == QStyleOptionTab::End) || (rtlHorTabs && o->position == QStyleOptionTab::Beginning));
			bool firstTab = ((!rtlHorTabs && o->position == QStyleOptionTab::Beginning) || (rtlHorTabs && o->position == QStyleOptionTab::End));
			bool onlyOne = o->position == QStyleOptionTab::OnlyOneTab;
			bool previousSelected = ((!rtlHorTabs && o->selectedPosition == QStyleOptionTab::PreviousIsSelected) || (rtlHorTabs && o->selectedPosition == QStyleOptionTab::NextIsSelected));
			bool nextSelected = ((!rtlHorTabs && o->selectedPosition == QStyleOptionTab::NextIsSelected) || (rtlHorTabs && o->selectedPosition == QStyleOptionTab::PreviousIsSelected));
			int tabBarAlignment = proxy()->styleHint(SH_TabBar_Alignment, o, widget);
			bool leftAligned = (!rtlHorTabs && tabBarAlignment == Qt::AlignLeft) || (rtlHorTabs && tabBarAlignment == Qt::AlignRight);
			bool rightAligned = (!rtlHorTabs && tabBarAlignment == Qt::AlignRight) || (rtlHorTabs && tabBarAlignment == Qt::AlignLeft);

			QColor light = o->palette.light().color();
//			QColor dark = o->palette.dark().color();
			QColor shadow = o->palette.shadow().color();
			int borderThinkness = proxy()->pixelMetric(PM_TabBarBaseOverlap, o, widget);
			if (selected) {
				borderThinkness /= 2;
			}
			QRect r2(option->rect);
			int x1 = r2.left();
			int x2 = r2.right();
			int y1 = r2.top();
			int y2 = r2.bottom();
			switch (o->shape) {
			default:
				QProxyStyle::drawControl(ce, o, p, widget);
				break;
			case QTabBar::RoundedNorth:
			{
#ifdef Q_OS_WIN
				if (selected) {
					y2 += 2;
				} else {
					y2 += 1;
					y1 += 2;
					x1 += onlyOne || firstTab ? borderThinkness : 0;
					x2 -= onlyOne || lastTab ? borderThinkness : 0;
				}
#elif 1
				if (selected) {
					y2 += 2;
				} else {
					y2 += 1;
					y1 += 2;
					x1 += onlyOne || firstTab ? borderThinkness : 0;
					x2 -= onlyOne || lastTab ? borderThinkness : 0;
				}

#else
				if (!selected) {
					y1 += 2;
					x1 += onlyOne || firstTab ? borderThinkness : 0;
					x2 -= onlyOne || lastTab ? borderThinkness : 0;
				}
#endif

				p->fillRect(QRect(x1 + 1, y1 + 1, (x2 - x1) - 1, (y2 - y1) - 2), o->palette.background());

				// Left
				if (firstTab || selected || onlyOne || !previousSelected) {
					p->setPen(light);
					p->drawLine(x1, y1 + 2, x1, y2 - 1 - ((onlyOne || firstTab) && selected && leftAligned ? 0 : borderThinkness));
					p->drawPoint(x1 + 1, y1 + 1);
				}
				// Top
				{
					int beg = x1 + (previousSelected ? 0 : 2);
					int end = x2 - (nextSelected ? 0 : 2);
					p->setPen(light);
					p->drawLine(beg, y1, end, y1);
				}
				// Right
				if (lastTab || selected || onlyOne || !nextSelected) {
					p->setPen(shadow);
					p->drawLine(x2, y1 + 2, x2, y2 - 1 - ((onlyOne || lastTab) && selected && rightAligned ? 0 : borderThinkness));
					p->drawPoint(x2 - 1, y1 + 1);
				}
				break;
			}
			case QTabBar::RoundedSouth:
			{
#ifdef Q_OS_WIN
				if (selected) {
					y1 -= 2;
				} else {
					x1 -= 1;
					y1 -= 1;
					y2 -= 2;
					x1 += firstTab ? borderThinkness : 0;
					x2 -= lastTab ? borderThinkness : 0;
				}
#elif 1
				if (selected) {
					y1 -= 2;
				} else {
					y1 -= 1;
					y2 -= 2;
					x1 += firstTab ? borderThinkness : 0;
					x2 -= lastTab ? borderThinkness : 0;
				}
#else
				if (!selected) {
					y2 -= 2;
					x1 += firstTab ? borderThinkness : 0;
					x2 -= lastTab ? borderThinkness : 0;
				}
#endif

				p->fillRect(QRect(x1 + 1, y1 + 2, (x2 - x1) - 1, (y2 - y1) - 1), o->palette.background());

				// Left
				if (firstTab || selected || onlyOne || !previousSelected) {
					p->setPen(light);
					p->drawLine(x1, y2 - 2, x1, y1 + ((onlyOne || firstTab) && selected && leftAligned ? 0 : borderThinkness));
					p->drawPoint(x1 + 1, y2 - 1);
				}
				// Bottom
				{
					int beg = x1 + (previousSelected ? 0 : 2);
					int end = x2 - (nextSelected ? 0 : 2);
					p->setPen(shadow);
					p->drawLine(beg, y2, end, y2);
				}
				// Right
				if (lastTab || selected || onlyOne || !nextSelected) {
					p->setPen(shadow);
					p->drawLine(x2, y2 - 2, x2, y1 + ((onlyOne || lastTab) && selected && rightAligned ? 0 : borderThinkness));
					p->drawPoint(x2 - 1, y2 - 1);
				}
				break; }
			case QTabBar::RoundedWest:
			{
#ifdef Q_OS_WIN
				if (selected) {
					x2 += 1;
				} else {
					x1 += 2;
					y1 += firstTab ? borderThinkness : 0;
					y2 -= lastTab ? borderThinkness : 0;
				}
#elif 1
				if (selected) {
					x2 += 1;
				} else {
					x1 += 2;
					y1 += firstTab ? borderThinkness : 0;
					y2 -= lastTab ? borderThinkness : 0;
				}
#else
				if (!selected) {
					x1 += 2;
					y1 += firstTab ? borderThinkness : 0;
					y2 -= lastTab ? borderThinkness : 0;
				}
#endif

				p->fillRect(QRect(x1 + 1, y1 + 1, (x2 - x1) - 1, (y2 - y1) - 1), o->palette.background());

				// Top
				if (firstTab || selected || onlyOne || !previousSelected) {
					p->setPen(light);
					p->drawLine(x1 + 2, y1, x2 - ((onlyOne || firstTab) && selected && leftAligned ? 0 : borderThinkness), y1);
					p->drawPoint(x1 + 1, y1 + 1);
				}
				// Left
				{
					int beg = y1 + (previousSelected ? 0 : 2);
					int end = y2 - (nextSelected ? 0 : 2);
					p->setPen(light);
					p->drawLine(x1, beg, x1, end);
				}
				// Bottom
				if (lastTab || selected || onlyOne || !nextSelected) {
					p->setPen(shadow);
					p->drawLine(x1 + 2, y2, x2 - ((onlyOne || lastTab) && selected && rightAligned ? 0 : borderThinkness), y2);
					p->drawPoint(x1 + 1, y2 - 1);
				}
				break;
			}
			case QTabBar::RoundedEast:
			{
#ifdef Q_OS_WIN
				if (selected) {
					x1 -= 2;
				} else {
					y1 -= 2;
					x1 -= 1;
					x2 -= 2;
					y1 += firstTab ? borderThinkness : 0;
					y2 -= lastTab ? borderThinkness : 0;
				}
#elif 1
				if (selected) {
					x1 -= 2;
				} else {
					x1 -= 1;
					x2 -= 2;
					y1 += firstTab ? borderThinkness : 0;
					y2 -= lastTab ? borderThinkness : 0;
				}
#else
				if (!selected) {
					x2 -= 2;
					y1 += firstTab ? borderThinkness : 0;
					y2 -= lastTab ? borderThinkness : 0;
				}
#endif

				p->fillRect(QRect(x1 + 2, y1 + 1, (x2 - x1) - 1, (y2 - y1) - 1), o->palette.background());

				// Top
				if (firstTab || selected || onlyOne || !previousSelected) {
					p->setPen(light);
					p->drawLine(x2 - 2, y1, x1 - 1 + ((onlyOne || firstTab) && selected && leftAligned ? 0 : borderThinkness), y1);
					p->drawPoint(x2 - 1, y1 + 1);
				}
				// Right
				{
					int beg = y1 + (previousSelected ? 0 : 2);
					int end = y2 - (nextSelected ? 0 : 2);
					p->setPen(shadow);
					p->drawLine(x2, beg, x2, end);
				}
				// Bottom
				if (lastTab || selected || onlyOne || !nextSelected) {
					p->setPen(shadow);
					p->drawLine(x2 - 2, y2, x1 - 1 + ((onlyOne || lastTab) && selected && rightAligned ? 0 : borderThinkness), y2);
					p->drawPoint(x2 - 1, y2 - 1);
				}
				break;
			}
			}
		}
#endif
		return;
	}
	if (ce == CE_ProgressBarGroove || ce == CE_ProgressBarContents) {
		if (auto const *o = qstyleoption_cast<QStyleOptionProgressBar const *>(option)) {

			QColor color(0, 128, 255);

			bool horz = (o->orientation == Qt::Horizontal);

			QString key;
			QImage const *image;

			if (horz) {
				key = "horz";
				image = &m->progress_horz;
			} else {
				key = "vert";
				image = &m->progress_vert;
			}

			p->save();

			int x = option->rect.x();
			int y = option->rect.y();
			int w = option->rect.width();
			int h = option->rect.height();
			if (ce == CE_ProgressBarContents) {
				int len = o->progress - o->minimum;
				int div = o->maximum - o->minimum;
				bool inv = o->invertedAppearance;
				if (horz) {
					int v = div > 0 ? (w * len / div) : 0;
					if (inv) {
						x = x + w - v;
					}
					w = v;
				} else {
					int v = div > 0 ? (h * len / div) : 0;
					if (!inv) {
						y = y + h - v;
					}
					h = v;
				}
				QRect r(x, y, w, h);
				p->setClipRect(r);
				p->fillRect(option->rect, color);
			}
			QPixmap pm;
			QSize size(w, h);
			key = pixmapkey("progress_bar", key, size, m->base_color);
			if (!QPixmapCache::find(key, &pm)) {
				QImage im = image->scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
				pm = QPixmap::fromImage(im);
				QPixmapCache::insert(key, pm);
			}
			p->setOpacity(0.5);
			p->drawPixmap(option->rect, pm, pm.rect());

			p->restore();
		}
		return;
	}
	if (ce == CE_HeaderSection || ce == CE_HeaderEmptyArea) {
		bool horz = true;
		if (auto const *o = qstyleoption_cast<QStyleOptionHeader const *>(option)) {
			horz = (o->orientation == Qt::Horizontal);
		}
		int x = option->rect.x();
		int y = option->rect.y();
		int w = option->rect.width();
		int h = option->rect.height();
		QLinearGradient gradient;
		gradient.setStart(x, y);
		gradient.setFinalStop(x, y + h / 4.0);
		gradient.setColorAt(0, option->palette.color(QPalette::Light));
		gradient.setColorAt(1, option->palette.color(QPalette::Window));
		p->fillRect(x, y, w, h, gradient);
		p->fillRect(x, y + h - 1, w, 1, option->palette.color(QPalette::Dark));
		if (horz) {
			p->fillRect(x + w - 1, y, 1, h, option->palette.color(QPalette::Dark));
		}
		if (ce == CE_HeaderSection) {
			if (horz) {
				p->fillRect(x, y, 1, h, option->palette.color(QPalette::Light));
			}
			if (option->state & QStyle::State_MouseOver) {
				p->save();
				p->fillRect(x, y, w, h, QColor(255, 255, 255, 32));
				p->restore();
			}
		}
		return;
	}
	if (ce == CE_HeaderLabel) {
		if (auto const *o = qstyleoption_cast<QStyleOptionHeader const *>(option)) {
			QRect rect = o->rect;
			if (!o->icon.isNull()) {
				int iconExtent = pixelMetric(PM_SmallIconSize, option, widget);
				QPixmap pixmap = o->icon.pixmap(QSize(iconExtent, iconExtent), (o->state & State_Enabled) ? QIcon::Normal : QIcon::Disabled);
				int pixw = int(pixmap.width() / pixmap.devicePixelRatio());

				QRect aligned = alignedRect(o->direction, QFlag(o->iconAlignment), pixmap.size() / pixmap.devicePixelRatio(), rect);
				QRect inter = aligned.intersected(rect);
				p->drawPixmap(inter.x(), inter.y(), pixmap, inter.x() - aligned.x(), inter.y() - aligned.y(), int(aligned.width() * pixmap.devicePixelRatio()), int(pixmap.height() * pixmap.devicePixelRatio()));

				const int margin = pixelMetric(QStyle::PM_HeaderMargin, option, widget);
				if (o->direction == Qt::LeftToRight) {
					rect.setLeft(rect.left() + pixw + margin);
				} else {
					rect.setRight(rect.right() - pixw - margin);
				}
			}
			if (o->state & QStyle::State_On) {
				QFont fnt = p->font();
				fnt.setBold(true);
				p->setFont(fnt);
			}
			int leftmargin = 0;
			auto align = o->textAlignment;
			if (align & Qt::AlignLeft) {
				leftmargin += 4;
			}
			drawItemText(p, rect.adjusted(leftmargin, 0, 0, 0), align, o->palette, (o->state & State_Enabled), o->text, QPalette::ButtonText);
		}
		return;
	}
#ifdef Q_OS_MAC
	if (ce == CE_DockWidgetTitle) {
		if (auto const *o = qstyleoption_cast<QStyleOptionDockWidget const *>(option)) {
			auto const *dockWidget = qobject_cast<QDockWidget const *>(widget);
			QRect rect = option->rect;
			if (dockWidget && dockWidget->isFloating()) {
				QProxyStyle::drawControl(ce, option, p, widget);
				return;
			}

			auto const *v2 = qstyleoption_cast<const QStyleOptionDockWidgetV2*>(o);
			bool verticalTitleBar = v2 && v2->verticalTitleBar;

			if (verticalTitleBar) {
				rect.setSize(rect.size().transposed());

				p->translate(rect.left() - 1, rect.top() + rect.width());
				p->rotate(-90);
				p->translate(-rect.left() + 1, -rect.top());
			}

			p->setBrush(option->palette.background().color().darker(110));
			p->setPen(option->palette.background().color().darker(130));
			p->drawRect(rect.adjusted(0, 1, -1, -3));

			int buttonMargin = 4;
			int mw = proxy()->pixelMetric(QStyle::PM_DockWidgetTitleMargin, o, widget);
			int fw = proxy()->pixelMetric(PM_DockWidgetFrameWidth, o, widget);
			auto const *dw = qobject_cast<const QDockWidget *>(widget);
			bool isFloating = dw && dw->isFloating();

			QRect r = option->rect.adjusted(0, 2, -1, -3);
			QRect titleRect = r;

			if (o->closable) {
				QSize sz = proxy()->standardIcon(QStyle::SP_TitleBarCloseButton, o, widget).actualSize(QSize(10, 10));
				titleRect.adjust(0, 0, -sz.width() - mw - buttonMargin, 0);
			}

			if (o->floatable) {
				QSize sz = proxy()->standardIcon(QStyle::SP_TitleBarMaxButton, o, widget).actualSize(QSize(10, 10));
				titleRect.adjust(0, 0, -sz.width() - mw - buttonMargin, 0);
			}

			if (isFloating) {
				titleRect.adjust(0, -fw, 0, 0);
				if (widget && widget->windowIcon().cacheKey() != QApplication::windowIcon().cacheKey()) {
					titleRect.adjust(titleRect.height() + mw, 0, 0, 0);
				}
			} else {
				titleRect.adjust(mw, 0, 0, 0);
				if (!o->floatable && !o->closable) {
					titleRect.adjust(0, 0, -mw, 0);
				}
			}
			if (!verticalTitleBar) {
				titleRect = visualRect(o->direction, r, titleRect);
			}

			if (!o->title.isEmpty()) {
				QString titleText = p->fontMetrics().elidedText(o->title, Qt::ElideRight, verticalTitleBar ? titleRect.height() : titleRect.width());
				const int indent = 4;
				int align = Qt::AlignRight | Qt::AlignVCenter;
				drawItemText(p, rect.adjusted(indent + 1, 1, -indent - 1, -1), align, o->palette, o->state & State_Enabled, titleText, QPalette::WindowText);
			}
		}
		return;
	}
#endif // Q_OS_MAC
#ifdef Q_OS_LINUX
	if (ce == CE_Splitter) {
		p->fillRect(option->rect, option->palette.color(QPalette::Window));
		return;
	}
#endif
	if (ce == CE_Header) {
		drawControl(CE_HeaderSection, option, p, widget);
		drawControl(CE_HeaderLabel, option, p, widget);
		return;
	}
	if (ce == CE_ItemViewItem) {
		if (auto const *o = qstyleoption_cast<QStyleOptionViewItem const *>(option)) {
			p->save();
			p->setFont(o->font);
			drawPrimitive(PE_PanelItemViewItem, option, p, widget);
			drawItemText(p, o->rect.adjusted(2, 0, 0, 0), o->displayAlignment, option->palette, true, o->text);
			p->restore();
			return;
		}
	}
	if (ce == CE_TabBarTab) {
		drawControl(CE_TabBarTabShape, option, p, widget);
		drawControl(CE_TabBarTabLabel, option, p, widget);
		return;
	}
	if (ce == CE_PushButton) {
		drawControl(CE_PushButtonBevel, option, p, widget);
		drawControl(CE_PushButtonLabel, option, p, widget);
		return;
	}
	if (ce == CE_ProgressBar) {
		drawControl(CE_ProgressBarGroove, option, p, widget);
		drawControl(CE_ProgressBarContents, option, p, widget);
		return;
	}
//	qDebug() << ce;
	QProxyStyle::drawControl(ce, option, p, widget);
}

void DarkStyle::drawComplexControl(ComplexControl cc, const QStyleOptionComplex *option, QPainter *p, const QWidget *widget) const
{
//	qDebug() << cc;
	if (cc == QStyle::CC_ComboBox) {
		if (auto const *o = qstyleoption_cast<QStyleOptionComboBox const *>(option)) {
			SubControls sub = option->subControls;
			if (o->editable) {
#define EP_EDITBORDER_NOSCROLL      6
#define EP_EDITBORDER_HVSCROLL      9
				if (sub & SC_ComboBoxEditField) {
					p->fillRect(option->rect, option->palette.color(QPalette::Dark));
					drawFrame(p, option->rect, option->palette.color(QPalette::Shadow), option->palette.color(QPalette::Light));
				}
				if (sub & SC_ComboBoxArrow) {
					QRect subRect = subControlRect(CC_ComboBox, option, SC_ComboBoxArrow, widget);
					QStyleOptionComplex opt = *option;
					opt.rect = subRect;
					drawToolButton(p, &opt);
					opt.rect = QRect(subRect.right() - 4, subRect.y() + subRect.height() / 2 - 1, 2, 2);
					drawPrimitive(PE_IndicatorArrowDown, &opt, p, widget);
				}

			} else {
				if (sub & SC_ComboBoxFrame) {
					QStyleOptionButton btn;
					btn.QStyleOption::operator=(*option);
					btn.rect = option->rect.adjusted(-1, -1, 1, 1);
					if (sub & SC_ComboBoxArrow) {
						btn.features = QStyleOptionButton::HasMenu;
					}
					drawControl(QStyle::CE_PushButton, &btn, p, widget);
				}
			}
		}
		return;
	}
	if (cc == QStyle::CC_ToolButton) {
		QStyle::State flags = option->state;
		if (auto const *o = qstyleoption_cast<QStyleOptionToolButton const *>(option)) {
			QRect button, menuarea;
			button = subControlRect(cc, o, SC_ToolButton, widget);
			menuarea = subControlRect(cc, o, SC_ToolButtonMenu, widget);

			State bflags = o->state & ~State_Sunken;
			State mflags = bflags;
			bool autoRaise = flags & State_AutoRaise;
			if (autoRaise) {
				if (!(bflags & State_MouseOver) || !(bflags & State_Enabled)) {
					bflags &= ~State_Raised;
				}
			}

			if (o->state & State_Sunken) {
				if (o->activeSubControls & SC_ToolButton) {
					bflags |= State_Sunken;
					mflags |= State_MouseOver | State_Sunken;
				} else if (o->activeSubControls & SC_ToolButtonMenu) {
					mflags |= State_Sunken;
					bflags |= State_MouseOver;
				}
			}

			QStyleOption tool(0);
			tool.palette = o->palette;
			if (o->subControls & SC_ToolButton) {
				if (flags & (State_Sunken | State_On | State_Raised) || !autoRaise) {
					tool.rect = option->rect;
					tool.state = bflags;
					drawToolButton(p, &tool);
				}
			}

			if (o->state & State_HasFocus) {
				QStyleOptionFocusRect fr;
				fr.QStyleOption::operator=(*o);
				fr.rect.adjust(3, 3, -3, -3);
				if (o->features & QStyleOptionToolButton::MenuButtonPopup) {
					fr.rect.adjust(0, 0, -pixelMetric(QStyle::PM_MenuButtonIndicator, o, widget), 0);
				}
				drawPrimitive(PE_FrameFocusRect, &fr, p, widget);
			}
			QStyleOptionToolButton label = *o;
			label.state = bflags;
			int fw = 2;
			if (!autoRaise)
				label.state &= ~State_Sunken;
			label.rect = button.adjusted(fw, fw, -fw, -fw);
			drawControl(CE_ToolButtonLabel, &label, p, widget);

			if (o->subControls & SC_ToolButtonMenu) {
				tool.rect = menuarea;
				tool.state = mflags;
				if (autoRaise) {
					drawPrimitive(PE_IndicatorButtonDropDown, &tool, p, widget);
				} else {
					tool.state = mflags;
					menuarea.adjust(-2, 0, 0, 0);
					// Draw menu button
					if ((bflags & State_Sunken) != (mflags & State_Sunken)){
						p->save();
						p->setClipRect(menuarea);
						tool.rect = option->rect;
						drawToolButton(p, &tool);
						p->restore();
					}
					// Draw arrow
					p->save();
					p->setPen(option->palette.dark().color());
					p->drawLine(menuarea.left(), menuarea.top() + 3, menuarea.left(), menuarea.bottom() - 3);
					p->setPen(option->palette.light().color());
					p->drawLine(menuarea.left() - 1, menuarea.top() + 3, menuarea.left() - 1, menuarea.bottom() - 3);

					tool.rect = menuarea.adjusted(2, 3, -2, -1);
					drawPrimitive(PE_IndicatorArrowDown, &tool, p, widget);
					p->restore();
				}
			} else if (o->features & QStyleOptionToolButton::HasMenu) {
				int mbi = pixelMetric(PM_MenuButtonIndicator, o, widget);
				QRect ir = o->rect;
				QStyleOptionToolButton newBtn = *o;
				newBtn.rect = QRect(ir.right() + 4 - mbi, ir.height() - mbi + 4, mbi - 5, mbi - 5);
				drawPrimitive(PE_IndicatorArrowDown, &newBtn, p, widget);
			}
		}
		return;
	}
	if (cc == QStyle::CC_ScrollBar) {
		bool ishorz = (option->state & State_Horizontal);
		auto *tx = const_cast<ScrollBarTextures *>(ishorz ? &m->hsb : &m->vsb);

		int extent = (ishorz ? tx->slider.im_normal.height() : tx->slider.im_normal.width()) - 2;

		auto DrawNinePatchImage2 = [&](QImage const &image, QRect const &r){
			int w, h;
			if (ishorz) {
				h = extent;
				int d = r.height();
				w = (d == 0) ? 0 : (h * r.width() / d);
			} else {
				w = extent;
				int d = r.width();
				h = (d == 0) ? 0 : (w * r.height() / d);
			}
			drawNinePatchImage(p, image, r, w, h);
		};

		p->fillRect(option->rect, color(64));
		p->setRenderHint(QPainter::Antialiasing);

		{
#ifdef Q_OS_MAC
			QRect r = option->rect;
#else
			QRect r1 = subControlRect(CC_ScrollBar, option, SC_ScrollBarAddPage, widget);
			QRect r2 = subControlRect(CC_ScrollBar, option, SC_ScrollBarSubPage, widget);
			QRect r = r1.united(r2);
#endif
			DrawNinePatchImage2(tx->page_bg, r);
		}

#ifndef Q_OS_MAC // macOSのスクロールバーには、ラインステップボタンが無い
		{
			QRect r = subControlRect(CC_ScrollBar, option, SC_ScrollBarSubLine, widget);
			DrawNinePatchImage2(tx->page_bg, r);
		}
		{
			QRect r = subControlRect(CC_ScrollBar, option, SC_ScrollBarAddLine, widget);
			DrawNinePatchImage2(tx->page_bg, r);
		}

		auto DrawAddSubButton = [&](ButtonImages const &ims, SubControl subctl){
			QRect r = subControlRect(CC_ScrollBar, option, subctl, widget);
			QPixmap pm;;
			if (!(option->activeSubControls & subctl)) {
				pm = pixmapFromImage(ims.im_normal, r.size());
				p->drawPixmap(r.topLeft(), pm);
			} else {
				pm = pixmapFromImage(ims.im_hover, r.size());
				p->drawPixmap(r.topLeft(), pm);
			}
		};
		DrawAddSubButton(tx->sub_line, SC_ScrollBarSubLine);
		DrawAddSubButton(tx->add_line, SC_ScrollBarAddLine);
#endif

		{
			QRect r = subControlRect(CC_ScrollBar, option, SC_ScrollBarSlider, widget);
			int w, h;
			if (ishorz) {
				h = extent;
				int d = r.height();
				w = (d == 0) ? 0 : (h * r.width() / d);
			} else {
				w = extent;
				int d = r.width();
				h = (d == 0) ? 0 : (w * r.height() / d);
			}
#ifdef Q_OS_MAC // macだとズレて見えるので調整する
			if (ishorz) {
				r = r.translated(1, 0);
			} else {
				r = r.translated(0, 1);
			}
#endif
			if (!(option->activeSubControls & SC_ScrollBarSlider)) {
				DrawNinePatchImage2(tx->slider.im_normal, r);
			} else {
				DrawNinePatchImage2(tx->slider.im_hover, r);
			}
		}

		return;
	}
	if (cc == CC_GroupBox) {
		if (auto const *o = qstyleoption_cast<QStyleOptionGroupBox const *>(option)) {
			QRect textRect = subControlRect(CC_GroupBox, option, SC_GroupBoxLabel, widget);
			QRect checkBoxRect = subControlRect(CC_GroupBox, option, SC_GroupBoxCheckBox, widget);
			if (o->subControls & QStyle::SC_GroupBoxFrame) {
				QStyleOptionFrame frame;
				frame.QStyleOption::operator=(*o);
				frame.features = o->features;
				frame.lineWidth = o->lineWidth;
				frame.midLineWidth = o->midLineWidth;
				frame.rect = subControlRect(CC_GroupBox, option, SC_GroupBoxFrame, widget);
				p->save();
				QRegion region(o->rect);
				if (!o->text.isEmpty()) {
					bool ltr = o->direction == Qt::LeftToRight;
					QRect finalRect;
					if (o->subControls & QStyle::SC_GroupBoxCheckBox) {
						finalRect = checkBoxRect.united(textRect);
						finalRect.adjust(ltr ? -4 : 0, 0, ltr ? 0 : 4, 0);
					} else {
						finalRect = textRect;
					}
					region -= finalRect;
				}
                p->setClipRegion(region);
                drawPrimitive(PE_FrameGroupBox, &frame, p, widget);
				p->restore();
			}
			// Draw title
			if ((o->subControls & QStyle::SC_GroupBoxLabel) && !o->text.isEmpty()) {
				QColor textColor = o->textColor;
				if (textColor.isValid()) {
					p->setPen(textColor);
				}
				int alignment = int(o->textAlignment);
				if (!styleHint(QStyle::SH_UnderlineShortcut, option, widget)) {
					alignment |= Qt::TextHideMnemonic;
				}

				alignment |= Qt::TextShowMnemonic;
				alignment |= Qt::AlignHCenter;
				drawItemText(p, textRect, alignment, o->palette, o->state & State_Enabled, o->text, QPalette::WindowText);

				if (o->state & State_HasFocus) {
					QStyleOptionFocusRect fropt;
					fropt.QStyleOption::operator=(*o);
					fropt.rect = textRect;
					drawPrimitive(PE_FrameFocusRect, &fropt, p, widget);
				}
			}

			// Draw checkbox
			if (o->subControls & SC_GroupBoxCheckBox) {
				QStyleOptionButton box;
				box.QStyleOption::operator=(*o);
				box.rect = checkBoxRect;
				drawPrimitive(PE_IndicatorCheckBox, &box, p, widget);
			}
		}
		return;
	}
	if (cc == CC_Slider) {
		if (auto const *o = qstyleoption_cast<QStyleOptionSlider const *>(option)) {
			QRect groove = subControlRect(CC_Slider, option, SC_SliderGroove, widget);
			QRect handle = subControlRect(CC_Slider, option, SC_SliderHandle, widget);

			bool horizontal = o->orientation == Qt::Horizontal;
			bool ticksAbove = o->tickPosition & QSlider::TicksAbove;
			bool ticksBelow = o->tickPosition & QSlider::TicksBelow;
			QPixmap cache;
			QBrush oldBrush = p->brush();
			QPen oldPen = p->pen();
			QColor shadowAlpha(Qt::black);
			shadowAlpha.setAlpha(10);

			if ((option->subControls & SC_SliderGroove) && groove.isValid()) {
				QRect rect(0, 0, groove.width(), groove.height());
				QString key = pixmapkey("slider_groove", horizontal ? "horz" : "vert", rect.size(), m->base_color);

				QRectF grooveRect;
				double r = std::min(groove.width(), groove.height()) / 2.0;
				{
					double n = r * 3 / 4;
					grooveRect = QRectF(rect).adjusted(n, n, -n, -n);
					r -= n;
				}

				// draw background groove
				QPixmap cache;
				if (!QPixmapCache::find(key, cache)) {
					cache = QPixmap(rect.size());
					cache.fill(Qt::transparent);
					QPainter groovePainter(&cache);
					groovePainter.setRenderHint(QPainter::Antialiasing, true);
					groovePainter.translate(0.5, 0.5);
					QLinearGradient gradient;
					if (horizontal) {
						gradient.setStart(grooveRect.center().x(), grooveRect.top());
						gradient.setFinalStop(grooveRect.center().x(), grooveRect.bottom());
					} else {
						gradient.setStart(grooveRect.left(), grooveRect.center().y());
						gradient.setFinalStop(grooveRect.right(), grooveRect.center().y());
					}
					groovePainter.setPen(Qt::NoPen);
					gradient.setColorAt(0, color(32));
					gradient.setColorAt(1, color(128));
					groovePainter.setBrush(gradient);
					groovePainter.drawRoundedRect(grooveRect, r, r);
					groovePainter.end();
					QPixmapCache::insert(key, cache);
				}
				p->drawPixmap(groove.topLeft(), cache);
			}

			if (option->subControls & SC_SliderTickmarks) {
				int tickSize = pixelMetric(PM_SliderTickmarkOffset, option, widget);
				int available = pixelMetric(PM_SliderSpaceAvailable, o, widget);
				int interval = o->tickInterval;
				if (interval <= 0) {
					interval = o->singleStep;
					if (QStyle::sliderPositionFromValue(o->minimum, o->maximum, interval, available) - QStyle::sliderPositionFromValue(o->minimum, o->maximum, 0, available) < 3) {
						interval = o->pageStep;
					}
				}
				if (interval <= 0) {
					interval = 1;
				}

				int v = o->minimum;
				int len = pixelMetric(PM_SliderLength, o, widget);
				while (v <= o->maximum + 1) {
					if (v == o->maximum + 1 && interval == 1) break;
					const int v2 = qMin(v, o->maximum);
					int pos = sliderPositionFromValue(o->minimum, o->maximum,
													  v2, (horizontal
														   ? o->rect.width()
														   : o->rect.height()) - len,
													  o->upsideDown) + len / 2;
					int extra = 2 - ((v2 == o->minimum || v2 == o->maximum) ? 1 : 0);

					if (horizontal) {
						if (ticksAbove) {
							p->setPen(color(128));
							p->drawLine(pos, o->rect.top() + extra, pos, o->rect.top() + tickSize);
						}
						if (ticksBelow) {
							p->setPen(color(0));
							p->drawLine(pos, o->rect.bottom() - extra, pos, o->rect.bottom() - tickSize);
						}
					} else {
						if (ticksAbove) {
							p->setPen(color(128));
							p->drawLine(o->rect.left() + extra, pos, o->rect.left() + tickSize, pos);
						}
						if (ticksBelow) {
							p->setPen(color(0));
							p->drawLine(o->rect.right() - extra, pos, o->rect.right() - tickSize, pos);
						}
					}
					// in the case where maximum is max int
					int nextInterval = v + interval;
					if (nextInterval < v) break;
					v = nextInterval;
				}
			}
			// draw handle
			if ((option->subControls & SC_SliderHandle) ) {
				QRect pixmapRect(0, 0, handle.width(), handle.height());
				QRect r = pixmapRect.adjusted(0, 0, -1, -1);
				QPainterPath path;
				path.addEllipse(r);
				QString handlePixmapName = pixmapkey(QLatin1String("slider_handle"), "", handle.size(), m->base_color);
				if (!QPixmapCache::find(handlePixmapName, cache)) {
					cache = QPixmap(handle.size());
					cache.fill(Qt::transparent);
					QPainter handlePainter(&cache);

					handlePainter.setRenderHint(QPainter::Antialiasing, true);
					handlePainter.translate(0.5, 0.5);

					QLinearGradient gradient;
					gradient.setStart(pixmapRect.topLeft());
					gradient.setFinalStop(pixmapRect.bottomRight());
					gradient.setColorAt(0, color(192));
					gradient.setColorAt(1, QColor(0, 0, 0));
					handlePainter.save();
					handlePainter.setClipPath(path);
					handlePainter.fillRect(r, gradient);

					QColor highlight_color = colorize(color(160).rgb(), 192, 255);

					handlePainter.setPen(QPen(highlight_color, 2));
					handlePainter.setBrush(Qt::NoBrush);
					handlePainter.drawEllipse(r.adjusted(0, 0, 2, 2));
					handlePainter.setPen(QPen(QColor(0, 0, 0), 2));
					handlePainter.setBrush(Qt::NoBrush);
					handlePainter.drawEllipse(r.adjusted(-2, -2, 0, 0));

					handlePainter.restore();
					handlePainter.end();
					QPixmapCache::insert(handlePixmapName, cache);
				}

				QPoint topleft = handle.topLeft();
				p->drawPixmap(topleft, cache);

				if ((option->state & State_HasFocus) && (option->state & State_KeyboardFocusChange)) {
					p->save();
					p->setClipPath(path.translated(topleft));
					p->fillRect(handle, QColor(255, 255, 255, 32));
					p->restore();
				}
			}
			p->setBrush(oldBrush);
			p->setPen(oldPen);
		}
		return;
	}
//	qDebug() << cc;
	QProxyStyle::drawComplexControl(cc, option, p, widget);
}

