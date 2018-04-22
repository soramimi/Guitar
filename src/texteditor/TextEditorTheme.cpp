#include "TextEditorTheme.h"

TextEditorTheme::TextEditorTheme()
{
}

TextEditorThemePtr TextEditorTheme::Light()
{
	TextEditorThemePtr t = TextEditorThemePtr(new TextEditorTheme());
	t->fg_default = QColor(0, 0, 0);
	t->bg_default = QColor(240, 240, 240);
	t->fg_line_number = QColor(96, 96, 96);
	t->bg_line_number = QColor(208, 208, 208);
	t->fg_cursor = QColor(0, 128, 255);
	t->bg_current_line = QColor(192, 192, 192);
	t->bg_current_line_number = QColor(176, 176, 176);
	t->bg_diff_unknown = QColor(208, 208, 208);
	t->bg_diff_add = QColor(192, 240, 192);
	t->bg_diff_del = QColor(240, 208, 208);
	return t;
}

TextEditorThemePtr TextEditorTheme::Dark()
{
	TextEditorThemePtr t = TextEditorThemePtr(new TextEditorTheme());
	t->fg_default = QColor(255, 255, 255);
	t->bg_default = QColor(48, 48, 48); //	t->bg_default = QColor(0, 0, 64);
	t->fg_line_number = QColor(176, 176, 176);
	t->bg_line_number = QColor(64, 64, 64);
	t->fg_cursor = QColor(0, 128, 255);
	t->bg_current_line = QColor(64, 64, 64);
	t->bg_current_line_number = QColor(96, 96, 96);
	t->bg_diff_unknown = QColor(0, 0, 0);
	t->bg_diff_add = QColor(0, 64, 0);
	t->bg_diff_del = QColor(80, 0, 0);
	return t;
}

