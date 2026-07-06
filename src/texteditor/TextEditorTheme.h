#ifndef TEXTEDITORTHEME_H
#define TEXTEDITORTHEME_H

#include <QColor>
#include <memory>

class TextEditorTheme;

using TextEditorThemePtr = std::shared_ptr<TextEditorTheme>;

class TextEditorTheme {
public:
	QColor fg_default;
	QColor bg_default;
	QColor fg_line_number;
	QColor bg_line_number;
	QColor fg_cursor;
	QColor bg_current;
	QColor bg_current_line;
	QColor bg_current_line_number;
	QColor bg_diff_unknown;
	QColor bg_diff_line_del;
	QColor bg_diff_char_del;
	QColor bg_diff_line_add;
	QColor bg_diff_char_add;
public:
	static TextEditorThemePtr Dark();
	static TextEditorThemePtr Light();
};

#endif // TEXTEDITORTHEME_H
