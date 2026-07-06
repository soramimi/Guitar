
#include "TextEditorTheme.h"
#include <memory>

TextEditorThemePtr TextEditorTheme::Light()
{
	TextEditorThemePtr t = std::make_shared<TextEditorTheme>();
	t->fg_default             = (QRgb)0x000000;
	t->bg_default             = (QRgb)0xf0f0f0;
	t->fg_line_number         = (QRgb)0x606060;
	t->bg_line_number         = (QRgb)0xd0d0d0;
	t->fg_cursor              = (QRgb)0x0050ff;
	t->bg_current_line        = (QRgb)0xc0c0c0;
	t->bg_current_line_number = (QRgb)0xb0b0b0;
	t->bg_diff_unknown        = (QRgb)0xc0c0c0;
	t->bg_diff_line_del       = (QRgb)0xf0d0d0;
	t->bg_diff_line_add       = (QRgb)0xc0f0c0;
	t->bg_diff_char_del       = (QRgb)0xf090c0;
	t->bg_diff_char_add       = (QRgb)0x80d0c0;
	return t;
}

TextEditorThemePtr TextEditorTheme::Dark()
{
	TextEditorThemePtr t = std::make_shared<TextEditorTheme>();
	t->fg_default             = (QRgb)0xffffff;
	t->bg_default             = (QRgb)0x303030;
	t->fg_line_number         = (QRgb)0xb0b0b0;
	t->bg_line_number         = (QRgb)0x404040;
	t->fg_cursor              = (QRgb)0x0050ff;
	t->bg_current_line_number = (QRgb)0x606060;
	t->bg_diff_unknown        = (QRgb)0x202020;
	t->bg_diff_line_del       = (QRgb)0x500000;
	t->bg_diff_line_add       = (QRgb)0x004000;
	t->bg_diff_char_del       = (QRgb)0xc03070;
	t->bg_diff_char_add       = (QRgb)0x00a040;
	return t;
}

