
#ifndef JOINPATH_H
#define JOINPATH_H

#include <string>
#include <vector>
#include <string_view>

namespace JoinPath {

template <typename T> static std::basic_string_view<T> trimquot(std::basic_string_view<T> s)
{
	if (s.size() < 2) return s;
	if (s.front() == '"' && s.back() == '"') {
		return s.substr(1, s.size() - 2);
	}
	return s;
}

template <typename T> static std::basic_string<T> joinpath(std::basic_string_view<T> left, std::basic_string_view<T> right)
{
	left = trimquot(left);
	while (!left.empty() && (left.back() == '/' || left.back() == '\\')) {
		left.remove_suffix(1);
	}
	right = trimquot(right);
	while (!right.empty() && (right.front() == '/' || right.front() == '\\')) {
		right.remove_prefix(1);
	}
	if (left.empty()) {
		return std::basic_string<T>(right);
	}
	std::basic_string<T> ret;
	ret.reserve(left.size() + 1 + right.size());
	ret.append(left.data(), left.size());
	ret.push_back('/');
	ret.append(right.data(), right.size());
	return ret;
}

}

static inline std::string joinpath(std::string_view const &left, std::string_view const &right)
{
	return JoinPath::joinpath(left, right);
}

static inline std::string operator / (std::string_view const &left, std::string_view const &right)
{
	return joinpath(left, right);
}

#ifdef QT_VERSION
// #include <QString>
static inline QString qjoinpath(char16_t const *left, char16_t const *right)
{
	auto s = JoinPath::joinpath<char16_t>(left, right);
	if (s.empty()) return QString();
	return QString::fromUtf16(s.data(), s.size());
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
