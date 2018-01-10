#include "DarkStyle.h"
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

QStyle *StandardTheme::newStyle()
{
	return new StandardStyle();
}

// DarkTheme

QStyle *DarkTheme::newStyle()
{
	return new DarkStyle();
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
