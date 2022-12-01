
#include "TextEditorTheme.h"
#include <memory>

#define COLOR(X) ((QRgb)0x##X)

TextEditorThemePtr TextEditorTheme::Light()
{
	TextEditorThemePtr t = std::make_shared<TextEditorTheme>();
	t->fg_default             = COLOR(000000);
	t->bg_default             = COLOR(f0f0f0);
	t->fg_line_number         = COLOR(606060);
	t->bg_line_number         = COLOR(d0d0d0);
	t->fg_cursor              = COLOR(0050ff);
	t->bg_current_line        = COLOR(c0c0c0);
	t->bg_current_line_number = COLOR(b0b0b0);
	t->bg_diff_unknown        = COLOR(d0d0d0);
	t->bg_diff_line_del       = COLOR(f0d0d0);
	t->bg_diff_line_add       = COLOR(c0f0c0);
	t->bg_diff_char_del       = COLOR(f090c0);
	t->bg_diff_char_add       = COLOR(80d0c0);
	return t;
}

TextEditorThemePtr TextEditorTheme::Dark()
{
	TextEditorThemePtr t = std::make_shared<TextEditorTheme>();
	t->fg_default             = COLOR(ffffff);
	t->bg_default             = COLOR(303030);
	t->fg_line_number         = COLOR(b0b0b0);
	t->bg_line_number         = COLOR(404040);
	t->fg_cursor              = COLOR(0050ff);
	t->bg_current_line_number = COLOR(606060);
	t->bg_diff_unknown        = COLOR(000000);
	t->bg_diff_line_del       = COLOR(500000);
	t->bg_diff_line_add       = COLOR(004000);
	t->bg_diff_char_del       = COLOR(c03070);
	t->bg_diff_char_add       = COLOR(00a040);
	return t;
}

