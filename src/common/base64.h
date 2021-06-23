
#ifndef BASE64_H_
#define BASE64_H_

#include <vector>
#include <string>

void base64_encode(char const *src, size_t length, std::vector<char> *out);
void base64_decode(char const *src, size_t length, std::vector<char> *out);
void base64_encode(std::vector<char> const *src, std::vector<char> *out);
void base64_decode(std::vector<char> const *src, std::vector<char> *out);
void base64_encode(char const *src, std::vector<char> *out);
void base64_decode(char const *src, std::vector<char> *out);
static inline std::string to_s_(std::vector<char> const *vec)
{
	if (!vec || vec->empty()) return std::string();
	return std::string((char const *)&(*vec)[0], vec->size());
}
static inline std::string base64_encode(std::string const &src)
{
	std::vector<char> vec;
	base64_encode((char const *)src.c_str(), src.size(), &vec);
	return to_s_(&vec);
}
static inline std::string base64_decode(std::string const &src)
{
	std::vector<char> vec;
	base64_decode((char const *)src.c_str(), src.size(), &vec);
	return to_s_(&vec);
}

#endif

