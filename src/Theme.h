#ifndef THEME_H
#define THEME_H

#include <memory>
#include "TextEditorTheme.h"

class QStyle;

class AbstractTheme {
public:
	TextEditorThemePtr text_editor_theme;

	AbstractTheme();
	virtual ~AbstractTheme();
	virtual QStyle *newStyle() = 0;
};

class StandardTheme : public AbstractTheme {
public:
	virtual QStyle *newStyle();

};

class DarkTheme : public AbstractTheme {
public:
	virtual QStyle *newStyle();

};

typedef std::shared_ptr<AbstractTheme> ThemePtr;

ThemePtr createStandardTheme();
ThemePtr createDarkTheme();

#endif // THEME_H
