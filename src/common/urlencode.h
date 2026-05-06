
#ifndef URLENCODE_H_
#define URLENCODE_H_

#include <string>

std::string url_encode(char const *str, char const *end);
std::string url_decode(char const *str, char const *end);

std::string url_encode(char const *str, size_t len);
std::string url_decode(char const *str, size_t len);

std::string url_encode(char const *str);
std::string url_decode(char const *str);

std::string url_encode(std::string const &str);
std::string url_decode(std::string const &str);

#endif
