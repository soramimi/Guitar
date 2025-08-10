#ifndef TOI_H
#define TOI_H

#include <cctype>
#include <string_view>

template <typename T>
static T toi(std::string_view const &s, size_t *consumed = nullptr)
{
	T n = 0;
	size_t i = 0;
	bool sign = false;
	if (!s.empty()) {
		while (i < s.size() && std::isspace((unsigned char)s[i])) {
			i++;
		}
		if (i < s.size()) {
			if (s[i] == '+') {
				i++;
			} else if (s[i] == '-') {
				sign = true;
				i++;
			}
		}
		while (i < s.size()) {
			if (s[i] < '0' || s[i] > '9') break;
			n = n * 10 + (s[i] - '0');
			i++;
		}
	}
	if (consumed) {
		*consumed = i;
	}
	return sign ? -n : n;
}

#endif // TOI_H
