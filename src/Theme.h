#ifndef THEME_H
#define THEME_H

#include <QImage>
#include <QPalette>
#include <memory>
#include "TextEditorTheme.h"

class QStyle;

class AbstractTheme {
public:
	TextEditorThemePtr text_editor_theme;
	QColor dialog_header_frame_bg;
	QColor diff_slider_normal_bg;
	QColor diff_slider_unknown_bg;
	QColor diff_slider_add_bg;
	QColor diff_slider_del_bg;
	QColor diff_slider_handle;

	AbstractTheme();
	virtual ~AbstractTheme();
	virtual QStyle *newStyle() = 0;
	virtual QImage graphColorMap() = 0;

	virtual QPixmap resource_clear_png() = 0;
	virtual QPixmap resource_maximize_png() = 0;
	virtual QPixmap resource_menu_png() = 0;
};

typedef std::shared_ptr<AbstractTheme> ThemePtr;

class StandardTheme : public AbstractTheme {
public:
	StandardTheme();
	QStyle *newStyle();
	QImage graphColorMap();
	QPixmap resource_clear_png();
	QPixmap resource_maximize_png();
	QPixmap resource_menu_png();
};
ThemePtr createStandardTheme();

// #ifdef USE_DARK_THEME

class DarkTheme : public AbstractTheme {
public:
	DarkTheme();
	QStyle *newStyle();
	QImage graphColorMap();
	QPixmap resource_clear_png();
	QPixmap resource_maximize_png();
	QPixmap resource_menu_png();
};
ThemePtr createDarkTheme();

// #endif


#endif // THEME_H
