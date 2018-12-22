#include "charvec.h"
#include <cstring>
#include <cstdlib>

void print(std::vector<char> *out, char c)
{
	out->push_back(c);
}

void print(std::vector<char> *out, char const *begin, char const *end)
{
	out->insert(out->end(), begin, end);
}

void print(std::vector<char> *out, char const *ptr, size_t len)
{
	print(out, ptr, ptr + len);
}

void print(std::vector<char> *out, char const *s)
{
	print(out, s, s + strlen(s));
}

void print(std::vector<char> *out, std::string const &s)
{
	print(out, s.c_str(), s.size());
}

void print(std::vector<char> *out, std::vector<char> const *in)
{
	if (in && !in->empty()) {
		char const *begin = &(*in)[0];
		char const *end = begin + in->size();
		print(out, begin, end);
	}
}

std::string to_stdstr(std::vector<char> const &vec)
{
	if (!vec.empty()) {
		char const *begin = &vec.at(0);
		char const *end = begin + vec.size();
		return std::string(begin, end);
	}
	return std::string();
}
