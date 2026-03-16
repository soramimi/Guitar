
#ifndef __HTMLENCODE_H
#define __HTMLENCODE_H

#include <string>
#include <string_view>
#include <vector>
#include <cstring>

class HtmlEncode {
public:

	static void vecprint(std::vector<char> *out, char c)
	{
		out->push_back(c);
	}

	static void vecprint(std::vector<char> *out, char const *s)
	{
		out->insert(out->end(), s, s + strlen(s));
	}

	static std::string_view to_string(std::vector<char> const &vec)
	{
		if (!vec.empty()) {
			return {vec.data(), vec.size()};
		}
		return {};
	}

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

	static std::string html_encode(char const *ptr, char const *end, bool utf8through)
	{
		std::vector<char> vec;
		vec.reserve((end - ptr) * 2);
		html_encode_(ptr, end, utf8through, &vec);
		return (std::string)to_string(vec);
	}

	static std::string html_decode(char const *ptr, char const *end)
	{
		std::vector<char> vec;
		vec.reserve((end - ptr) * 2);
		html_decode_(ptr, end, &vec);
		return (std::string)to_string(vec);
	}

	static std::string html_encode(char const *ptr, size_t len, bool utf8through)
	{
		return html_encode(ptr, ptr + len, utf8through);
	}

	static std::string html_decode(char const *ptr, size_t len)
	{
		return html_decode(ptr, ptr + len);
	}

	static std::string html_encode(char const *ptr, bool utf8through)
	{
		return html_encode(ptr, strlen(ptr), utf8through);
	}

	static std::string html_decode(char const *ptr)
	{
		return html_decode(ptr, strlen(ptr));
	}

	static std::string html_encode(std::string_view const &str, bool utf8through)
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

	static std::string html_decode(std::string_view const &str)
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

};

static inline std::string html_encode(char const *ptr, char const *end, bool utf8through = true)
{
	return HtmlEncode::html_encode(ptr, end, utf8through);
}

static inline std::string html_decode(char const *ptr, char const *end)
{
	return HtmlEncode::html_decode(ptr, end);
}

static inline std::string html_encode(char const *ptr, size_t len, bool utf8through = true)
{
	return HtmlEncode::html_encode(ptr, ptr + len, utf8through);
}

static inline std::string html_decode(char const *ptr, size_t len)
{
	return HtmlEncode::html_decode(ptr, ptr + len);
}

static inline std::string html_encode(char const *ptr, bool utf8through = true)
{
	return HtmlEncode::html_encode(ptr, strlen(ptr), utf8through);
}

static inline std::string html_decode(char const *ptr)
{
	return HtmlEncode::html_decode(ptr, strlen(ptr));
}

static inline std::string html_encode(std::string_view const &str, bool utf8through = true)
{
	return HtmlEncode::html_encode(str, utf8through);
}

static inline std::string html_decode(std::string_view const &str)
{
	return HtmlEncode::html_decode(str);
}

#endif
