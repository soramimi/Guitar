#include "joinpath.h"
#include <sstream>
#include <vector>
#include <string.h>

#ifdef WIN32
#pragma warning(disable:4996)
#endif

template <typename T> static inline void trimquot(T const **begin, T const **end)
{
	if (*begin + 1 < *end && (*begin)[0] == '"' && (*end)[-1] == '"') {
		(*begin)++;
		(*end)--;
	}
}

template <typename T, typename U> void joinpath_(T const *left, T const *right, U *vec)
{
	size_t llen = 0;
	size_t rlen = 0;
	if (left) {
		T const *leftend = left + std::char_traits<T>::length(left);
		trimquot(&left, &leftend);
		while (left < leftend && (leftend[-1] == '/' || leftend[-1] == '\\')) {
			leftend--;
		}
		llen = leftend - left;
	}
	if (right) {
		T const *rightend = right + std::char_traits<T>::length(right);
		trimquot(&right, &rightend);
		while (right < rightend && (right[0] == '/' || right[0] == '\\')) {
			right++;
		}
		rlen = rightend - right;
	}
	vec->resize(llen + 1 + rlen);
	if (llen > 0) {
		std::char_traits<T>::copy(&vec->at(0), left, llen);
	}
	vec->at(llen) = '/';
	if (rlen > 0) {
		std::char_traits<T>::copy(&vec->at(llen + 1), right, rlen);
	}
}

std::string joinpath(char const *left, char const *right)
{
	std::vector<char> vec;
	joinpath_(left, right, &vec);
	return std::string(vec.begin(), vec.end());
}

std::string joinpath(std::string const &left, std::string const &right)
{
	return joinpath(left.c_str(), right.c_str());
}

QString qjoinpath(ushort const *left, ushort const *right)
{
    std::vector<ushort> vec;
	joinpath_(left, right, &vec);
    if (vec.empty()) return QString();
    return QString::fromUtf16(&vec[0], vec.size());
}

