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
	QPalette palette;
	QColor frame_line_color;
	QColor frame_background_color;

	AbstractTheme();
	virtual ~AbstractTheme();
	virtual QStyle *newStyle() = 0;
	virtual QImage graphColorMap() = 0;

};

class StandardTheme : public AbstractTheme {
public:
	StandardTheme();
	QStyle *newStyle();
	QImage graphColorMap();
};

class DarkTheme : public AbstractTheme {
public:
	DarkTheme();
	QStyle *newStyle();
	QImage graphColorMap();
};

typedef std::shared_ptr<AbstractTheme> ThemePtr;

ThemePtr createStandardTheme();
ThemePtr createDarkTheme();

#endif // THEME_H
