
#include "urlencode.h"
#include "charvec.h"
#include <cstdlib>
#include <cstdio>
#include <cctype>
#include <cstring>
#ifdef WIN32
#pragma warning(disable:4996)
#endif


static void url_encode_(char const *ptr, char const *end, std::vector<char> *out)
{
	while (ptr < end) {
		int c = (unsigned char)*ptr;
		ptr++;
		if (isalnum(c) || strchr("_.-~", c)) {
			vecprint(out, (char)c);
		} else if (c == ' ') {
			vecprint(out, '+');
		} else {
			char tmp[10];
			sprintf(tmp, "%%%02X", c);
			vecprint(out, tmp[0]);
			vecprint(out, tmp[1]);
			vecprint(out, tmp[2]);
		}
	}
}

std::string url_encode(char const *str, char const *end)
{
	if (!str) {
		return std::string();
	}

	std::vector<char> out;
	out.reserve(end - str + 10);

	url_encode_(str, end, &out);

	return to_stdstr(out);
}

std::string url_encode(char const *str, size_t len)
{
	return url_encode(str, str + len);
}

std::string url_encode(char const *str)
{
	return url_encode(str, strlen(str));
}

std::string url_encode(std::string const &str)
{
	char const *begin = str.c_str();
	char const *end = begin + str.size();
	char const *ptr = begin;

	while (ptr < end) {
		int c = (unsigned char)*ptr;
		if (isalnum(c) || strchr("_.-~", c)) {
			// thru
		} else {
			break;
		}
		ptr++;
	}
	if (ptr == end) {
		return str;
	}

	std::vector<char> out;
	out.reserve(str.size() + 10);

	out.insert(out.end(), begin, ptr);
	url_encode_(ptr, end, &out);

	return to_stdstr(out);
}

static void url_decode_(char const *ptr, char const *end, std::vector<char> *out)
{
	while (ptr < end) {
		unsigned char c = (unsigned char)*ptr;
		ptr++;
		if (c == '+') {
			c = ' ';
		} else if (c == '%' && isxdigit((unsigned char)ptr[0]) && isxdigit((unsigned char)ptr[1])) {
			char tmp[3]; // '%XX'
			tmp[0] = ptr[0];
			tmp[1] = ptr[1];
			tmp[2] = 0;
			c = (unsigned char)strtol(tmp, nullptr, 16);
			ptr += 2;
		}
		vecprint(out, (char)c);
	}
}

std::string url_decode(char const *str, char const *end)
{
	if (!str) {
		return std::string();
	}

	std::vector<char> out;
	out.reserve(size_t(end - str + 10));

	url_decode_(str, end, &out);

	return to_stdstr(out);
}

std::string url_decode(char const *str, size_t len)
{
	return url_decode(str, str + len);
}

std::string url_decode(char const *str)
{
	return url_decode(str, strlen(str));
}

std::string url_decode(std::string const &str)
{
	char const *begin = str.c_str();
	char const *end = begin + str.size();
	char const *ptr = begin;

	while (ptr < end) {
		int c = *ptr & 0xff;
		if (c == '+' || c == '%') {
			break;
		}
		ptr++;
	}
	if (ptr == end) {
		return str;
	}


	std::vector<char> out;
	out.reserve(str.size() + 10);

	out.insert(out.end(), begin, ptr);
	url_decode_(ptr, end, &out);

	return to_stdstr(out);
}
