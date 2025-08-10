
#include "htmlencode.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <string>
#include <cctype>

#ifdef WIN32
#pragma warning(disable:4996)
#endif

namespace {

inline void vecprint(std::vector<char> *out, char c)
{
	out->push_back(c);
}

inline void vecprint(std::vector<char> *out, char const *s)
{
	out->insert(out->end(), s, s + strlen(s));
}

inline std::string_view to_string(std::vector<char> const &vec)
{
	if (!vec.empty()) {
		return {vec.data(), vec.size()};
	}
	return {};
}

} // namespace

/**
 * @brief html_encode_
 * @param ptr
 * @param end
 * @param utf8through 非ASCII文字を &# エンコードするなら false 、そのまま出力するなら true
 * @param vec
 */
static void html_encode_(char const *ptr, char const *end, bool utf8through, std::vector<char> *vec)
{
	while (ptr < end) {
		int c = *ptr & 0xff;
		ptr++;
		switch (c) {
		case '&':
			vecprint(vec, "&amp;");
			break;
		case '<':
			vecprint(vec, "&lt;");
			break;
		case '>':
			vecprint(vec, "&gt;");
			break;
		case '\"':
			vecprint(vec, "&quot;");
			break;
		case '\'':
			vecprint(vec, "&apos;");
			break;
		case '\t':
		case '\n':
			vecprint(vec, c);
			break;
		default:
			if (c < 0x80 ? (c < 0x20 || c == '\'') : !utf8through) {
				char tmp[10];
				sprintf(tmp, "&#%u;", c);
				vecprint(vec, tmp);
			} else {
				vecprint(vec, c);
			}
		}
	}
}

static void html_decode_(char const *ptr, char const *end, std::vector<char> *vec)
{
	while (ptr < end) {
		int c = *ptr & 0xff;
		ptr++;
		if (c == '&') {
			char const *next = strchr(ptr, ';');
			if (!next) {
				break;
			}
			std::string t(ptr, next);
			if (t[0] == '#') {
				c = atoi(t.c_str() + 1);
				vecprint(vec, c);
			} else if (t == "amp") {
				vecprint(vec, '&');
			} else if (t == "lt") {
				vecprint(vec, '<');
			} else if (t == "gt") {
				vecprint(vec, '>');
			} else if (t == "quot") {
				vecprint(vec, '\"');
			} else if (t == "apos") {
				vecprint(vec, '\'');
			}
			ptr = next + 1;
		} else {
			vecprint(vec, c);
		}
	}
}

std::string html_encode(char const *ptr, char const *end, bool utf8through)
{
	std::vector<char> vec;
	vec.reserve((end - ptr) * 2);
	html_encode_(ptr, end, utf8through, &vec);
	return (std::string)to_string(vec);
}

std::string html_decode(char const *ptr, char const *end)
{
	std::vector<char> vec;
	vec.reserve((end - ptr) * 2);
	html_decode_(ptr, end, &vec);
	return (std::string)to_string(vec);
}

std::string html_encode(char const *ptr, size_t len, bool utf8through)
{
	return html_encode(ptr, ptr + len, utf8through);
}

std::string html_decode(char const *ptr, size_t len)
{
	return html_decode(ptr, ptr + len);
}

std::string html_encode(char const *ptr, bool utf8through)
{
	return html_encode(ptr, strlen(ptr), utf8through);
}

std::string html_decode(char const *ptr)
{
	return html_decode(ptr, strlen(ptr));
}

std::string html_encode(std::string_view const &str, bool utf8through)
{
	char const *begin = str.data();
	char const *end = begin + str.size();
	char const *ptr = begin;
	while (ptr < end) {
		int c = (unsigned char)*ptr;
		if (isspace(c) || strchr("&<>\"\'", c)) {
			break;
		}
		ptr++;
	}
	if (ptr == end) {
		return (std::string)str;
	}
	std::vector<char> vec;
	vec.reserve(str.size() * 2);
	vec.insert(vec.end(), begin, ptr);
	html_encode_(ptr, end, utf8through, &vec);
	begin = &vec[0];
	end = begin + vec.size();
	return std::string(begin, end);
}

std::string html_decode(std::string_view const &str)
{
	char const *begin = str.data();
	char const *end = begin + str.size();
	char const *ptr = begin;
	while (ptr < end) {
		int c = (unsigned char)*ptr;
		if (c == '&') {
			break;
		}
		ptr++;
	}
	if (ptr == end) {
		return (std::string)str;
	}
	std::vector<char> vec;
	vec.reserve(str.size() * 2);
	vec.insert(vec.end(), begin, ptr);
	html_decode_(ptr, end, &vec);
	begin = &vec[0];
	end = begin + vec.size();
	return std::string(begin, end);
}

