
#ifndef __HTMLENCODE_H
#define __HTMLENCODE_H

#include <string>
#include <string_view>

std::string html_encode(char const *ptr, char const *end, bool utf8through = true);
std::string html_decode(char const *ptr, char const *end);

std::string html_encode(char const *ptr, size_t len, bool utf8through = true);
std::string html_decode(char const *ptr, size_t len);

std::string html_encode(char const *ptr, bool utf8through = true);
std::string html_decode(char const *ptr);

std::string html_encode(std::string_view const &str, bool utf8through = true);
std::string html_decode(std::string_view const &str);

#endif
