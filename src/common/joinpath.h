
#ifndef JOINPATH_H
#define JOINPATH_H

#include <string>
#include <vector>

class JoinPath {
public:

	template <typename T> static void trimquot(T const **begin, T const **end)
	{
		if (*begin + 1 < *end && (*begin)[0] == '"' && (*end)[-1] == '"') {
			(*begin)++;
			(*end)--;
		}
	}

	template <typename T, typename U> static void joinpath_(T const *left, T const *right, U *vec)
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

	static std::string joinpath(char const *left, char const *right)
	{
		std::vector<char> vec;
		joinpath_(left, right, &vec);
		return std::string(vec.begin(), vec.end());
	}

	static std::string joinpath(std::string const &left, std::string const &right)
	{
		return joinpath(left.c_str(), right.c_str());
	}

};

static inline std::string joinpath(char const *left, char const *right)
{
	return JoinPath::joinpath(left, right);
}

static inline std::string joinpath(std::string const &left, std::string const &right)
{
	return JoinPath::joinpath(left, right);
}

static inline std::string operator / (std::string const &left, std::string const &right)
{
	return joinpath(left, right);
}

#ifdef QT_VERSION
// #include <QString>
static inline QString qjoinpath(char16_t const *left, char16_t const *right)
{
	std::vector<char16_t> vec;
	JoinPath::joinpath_(left, right, &vec);
	if (vec.empty()) return QString();
	return QString::fromUtf16(&vec[0], vec.size());
}

inline QString joinpath(QString const &left, QString const &right)
{
	return qjoinpath((char16_t const *)left.utf16(), (char16_t const *)right.utf16());
}

static inline QString operator / (QString const &left, QString const &right)
{
	return joinpath(left, right);
}
#endif

#endif
