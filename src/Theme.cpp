#include "../darktheme/src/DarkStyle.h"
#include "Theme.h"

// StandardStyle

class StandardStyle : public QProxyStyle {
private:
	LegacyWindowsStyleTreeControl legacy_windows_;
public:
	StandardStyle()
		: QProxyStyle(0)
	{
	}
	void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget = 0) const
	{
		if (element == QStyle::PE_IndicatorBranch) {
			if (legacy_windows_.drawPrimitive(element, option, painter, widget)) {
				return;
			}
		}
		QProxyStyle::drawPrimitive(element, option, painter, widget);
	}
};

// AbstractTheme

AbstractTheme::AbstractTheme()
{

}

AbstractTheme::~AbstractTheme()
{

}

// StandardTheme

StandardTheme::StandardTheme()
{
	frame_line_color = palette.color(QPalette::Dark);
	frame_background_color = palette.color(QPalette::Light);
}

QStyle *StandardTheme::newStyle()
{
	return new StandardStyle();
}

QImage StandardTheme::graphColorMap()
{
	QImage image;
	image.load(":/image/graphcolor.png");
	return image;
}

// DarkTheme

DarkTheme::DarkTheme()
{
	palette = QPalette(QColor(64, 64, 64));
	frame_line_color = palette.color(QPalette::Light);
	frame_background_color = palette.color(QPalette::Dark);
}

QStyle *DarkTheme::newStyle()
{
	return new DarkStyle();
}

QImage DarkTheme::graphColorMap()
{
	QImage image;
	image.load(":/darktheme/graphcolor.png");
	return image;
}


ThemePtr createStandardTheme()
{
	AbstractTheme *p = new StandardTheme;
	p->text_editor_theme = TextEditorTheme::Light();
	return ThemePtr(p);
}

ThemePtr createDarkTheme()
{
	AbstractTheme *p = new DarkTheme;
	p->text_editor_theme = TextEditorTheme::Dark();
	return ThemePtr(p);
}
