#ifndef THEME_H
#define THEME_H

#include <QImage>
#include <QPalette>
#include <memory>
#include "TextEditorTheme.h"

#define USE_DARK_THEME

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

	AbstractTheme() = default;
	virtual ~AbstractTheme() = default;
	virtual QStyle *newStyle() = 0;
	virtual QImage graphColorMap() = 0;

	virtual QPixmap resource_clear_png() = 0;
	virtual QPixmap resource_maximize_png() = 0;
	virtual QPixmap resource_menu_png() = 0;
};

using ThemePtr = std::shared_ptr<AbstractTheme>;

class LightTheme : public AbstractTheme {
public:
	LightTheme() = default;
	QStyle *newStyle() override;
	QImage graphColorMap() override;
	QPixmap resource_clear_png() override;
	QPixmap resource_maximize_png() override;
	QPixmap resource_menu_png() override;
};
ThemePtr createLightTheme();

#ifdef USE_DARK_THEME

class DarkTheme : public AbstractTheme {
public:
	DarkTheme() = default;
	QStyle *newStyle() override;
	QImage graphColorMap() override;
	QPixmap resource_clear_png() override;
	QPixmap resource_maximize_png() override;
	QPixmap resource_menu_png() override;
};
ThemePtr createDarkTheme();

#endif


#endif // THEME_H
