#include "Theme.h"
#include <QApplication>
#include <QRgb>
#include <QProxyStyle>

// AbstractTheme





// LightTheme

// #include "darktheme/LightStyle.h"
#include "darktheme/DarkStyle.h"

QStyle *LightTheme::newStyle()
{
	auto *p = new DarkStyle(DarkStyle::Theme::Light);

	text_editor_theme = TextEditorTheme::Light();

	// dialog_header_frame_bg = Qt::white;
	diff_slider_normal_bg = QColor(224, 224, 224);
	diff_slider_unknown_bg = QColor(208, 208, 208);
	diff_slider_add_bg = QColor(64, 192, 64);
	diff_slider_del_bg = QColor(240, 64, 64);
	diff_slider_handle = Qt::black;

	return p;
}

QImage LightTheme::graphColorMap()
{
	QImage image;
	image.load(":/image/graphcolor.png");
	return image;
}

QIcon LightTheme::resource_clear_icon()
{
	return QIcon(":/image/clear.svg");
}

QPixmap LightTheme::resource_maximize_png()
{
	return QPixmap(":/image/maximize.png");
}

QPixmap LightTheme::resource_menu_png()
{
	return QPixmap(":/image/menu.png");
}

ThemePtr createLightTheme()
{
	AbstractTheme *p = new LightTheme;
	return ThemePtr(p);
}

#ifdef USE_DARK_THEME

#include "darktheme/DarkStyle.h"

// DarkTheme



QStyle *DarkTheme::newStyle()
{
	auto *p = new DarkStyle(DarkStyle::Theme::Dark);

	text_editor_theme = TextEditorTheme::Dark();

	// dialog_header_frame_bg =  QColor(32, 32, 32);
	diff_slider_normal_bg = QColor(32, 32, 32);
	diff_slider_unknown_bg = QColor(0, 0, 0);
	diff_slider_add_bg = QColor(0, 144, 0);
	diff_slider_del_bg = QColor(160, 0, 0);
	diff_slider_handle = QColor(255, 255, 255);

	return p;
}

QImage DarkTheme::graphColorMap()
{
	QImage image;
	image.load(":/darktheme/graphcolor.png");
	return image;
}

static QImage loadInvertedImage(QString const &path)
{
	QImage img(path);
	int w = img.width();
	int h = img.height();
	for (int y = 0; y < h; y++) {
		QRgb *p = (QRgb *)img.scanLine(y);
		for (int x = 0; x < w; x++) {
			int r = qRed(*p);
			int g = qGreen(*p);
			int b = qBlue(*p);
			int a = qAlpha(*p);
			*p = qRgba(255 - r, 255 - g, 255 - b, a);
			p++;
		}
	}
	return img;
}

QIcon DarkTheme::resource_clear_icon()
{
	return QIcon(":/image/clear.svg");
}

QPixmap DarkTheme::resource_maximize_png()
{
	QImage img = loadInvertedImage(":/image/maximize.png");
	return QPixmap::fromImage(img);
}

QPixmap DarkTheme::resource_menu_png()
{
	QImage img = loadInvertedImage(":/image/menu.png");
	return QPixmap::fromImage(img);
}

ThemePtr createDarkTheme()
{
	AbstractTheme *p = new DarkTheme;
	return ThemePtr(p);
}

#endif // USE_DAR_THEME
