#include "charvec.h"
#include <cstring>
#include <cstdlib>

void vecprint(std::vector<char> *out, char c)
{
	out->push_back(c);
}

void vecprint(std::vector<char> *out, char const *begin, char const *end)
{
	out->insert(out->end(), begin, end);
}

void vecprint(std::vector<char> *out, char const *ptr, size_t len)
{
	vecprint(out, ptr, ptr + len);
}

void vecprint(std::vector<char> *out, char const *s)
{
	vecprint(out, s, s + strlen(s));
}

void vecprint(std::vector<char> *out, std::string const &s)
{
	vecprint(out, s.c_str(), s.size());
}

void vecprint(std::vector<char> *out, std::vector<char> const *in)
{
	if (in && !in->empty()) {
		char const *begin = &(*in)[0];
		char const *end = begin + in->size();
		vecprint(out, begin, end);
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
