
#include <cstdarg>
#include <cstdio>
#include <cctype>
#include <cstdlib>
#include <string>

void x_logprintf(char const *file, int line, const char *format, ...)
{

	va_list ap;
	va_start(ap, format);
	char text[1000];
	int n = vsprintf(text, format, ap);
	while (n > 0 && isspace((unsigned char)text[n - 1])) n--;
	text[n] = 0;
	va_end(ap);

	std::string s = text;
	sprintf(text, "  (%s # %d)\n", text, file, line);
	s += text;
	fputs(s.c_str(), stderr);
}

