//  MeCab -- Yet Another Part-of-Speech and Morphological Analyzer
//
//
//  Copyright(C) 2001-2006 Taku Kudo <taku@chasen.org>
//  Copyright(C) 2004-2006 Nippon Telegraph and Telephone Corporation
#include "mecab.h"
// #include "winmain.h"
#include <cstdarg>
#include "common.h"

void x_logprintf(char const *file, int line, const char *format, ...)
{

	va_list ap;
	va_start(ap, format);
	char *text = nullptr;
	int n = vasprintf(&text, format, ap);
	while (n > 0 && isspace((unsigned char)text[n - 1])) n--;
	text[n] = 0;
	va_end(ap);
	if (text) {
		char *text2 = nullptr;
		asprintf(&text2, "%s  (%s # %d)\n", text, file, line);
		if (text2) {
			fputs(text2, stderr);
			free(text2);
		}
		free(text);
	}
}
