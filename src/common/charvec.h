
#ifndef CHARVEC_H
#define CHARVEC_H

#include <vector>
#include <string>
#include <cstring>

class VectorPrint {
public:
	static void vecprint(std::vector<char> *out, char c)
	{
		out->push_back(c);
	}

	static void vecprint(std::vector<char> *out, char const *begin, char const *end)
	{
		out->insert(out->end(), begin, end);
	}

	static void vecprint(std::vector<char> *out, char const *ptr, size_t len)
	{
		vecprint(out, ptr, ptr + len);
	}

	static void vecprint(std::vector<char> *out, char const *s)
	{
		vecprint(out, s, s + strlen(s));
	}

	static void vecprint(std::vector<char> *out, std::string const &s)
	{
		vecprint(out, s.c_str(), s.size());
	}

	static void vecprint(std::vector<char> *out, std::vector<char> const *in)
	{
		if (in && !in->empty()) {
			char const *begin = &(*in)[0];
			char const *end = begin + in->size();
			vecprint(out, begin, end);
		}
	}

	static std::string to_stdstr(std::vector<char> const &vec)
	{
		if (!vec.empty()) {
			char const *begin = &vec.at(0);
			char const *end = begin + vec.size();
			return std::string(begin, end);
		}
		return std::string();
	}
};

static inline void vecprint(std::vector<char> *out, char c)
{
	VectorPrint::vecprint(out, c);
}

static inline void vecprint(std::vector<char> *out, char const *begin, char const *end)
{
	VectorPrint::vecprint(out, begin, end);
}

static inline void vecprint(std::vector<char> *out, char const *ptr, size_t len)
{
	VectorPrint::vecprint(out, ptr, len);
}

static inline void vecprint(std::vector<char> *out, char const *s)
{
	VectorPrint::vecprint(out, s);
}

static inline void vecprint(std::vector<char> *out, std::string const &s)
{
	VectorPrint::vecprint(out, s);
}

static inline void vecprint(std::vector<char> *out, std::vector<char> const *in)
{
	VectorPrint::vecprint(out, in);
}

static inline std::string to_stdstr(std::vector<char> const &vec)
{
	return VectorPrint::to_stdstr(vec);
}

#endif
