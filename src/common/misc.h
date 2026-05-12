#ifndef MISC_H
#define MISC_H

#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#else
#include <strings.h>
#endif

#ifdef APP_GUITAR
#include <QString>
#endif

namespace misc {

static inline int stricmp(char const *s1, char const *s2)
{
#ifdef _WIN32
	return ::stricmp(s1, s2);
#else
	return ::strcasecmp(s1, s2);
#endif
}

static inline int strnicmp(char const *s1, char const *s2, size_t n)
{
#ifdef _WIN32
	return ::strnicmp(s1, s2, n);
#else
	return ::strncasecmp(s1, s2, n);
#endif
}

static inline int stricmp(std::string_view const &s1, std::string_view const &s2)
{
	size_t n1 = s1.size();
	size_t n2 = s2.size();
	if (n1 == n2) {
		return strnicmp(s1.data(), s2.data(), n1);
	}
	return n1 < n2 ? -1 : 1;
}

static inline char const *stristr(const char *haystack, const char *needle)
{
	size_t needle_len = strlen(needle);
	for (const char *p = haystack; *p; p++) {
		if (strnicmp(p, needle, needle_len) == 0) {
			return p;
		}
	}
	return nullptr;
}

#ifdef QT_VERSION
QString normalizePathSeparator(QString const &str);
#endif

std::vector<std::string_view> splitLinesV(std::string_view const &str);
std::vector<std::string_view> splitLinesV(std::vector<char> const &ba);
std::vector<std::string_view> splitLinesKeepNewLineV(std::string_view const &str);
std::vector<std::string> splitLines(std::string_view const &str);
#ifdef QT_VERSION
std::vector<QString> splitLines(QString const &text);
#endif

std::vector<std::string_view> splitWords(std::string_view const &text);
std::vector<std::string_view> split(std::string_view const &sv, char sep);
std::string filename(std::string const &path);
bool starts_with(const std::string_view &str, const std::string_view &with);
bool starts_with(const std::string_view &str, char with);
bool ends_with(const std::string_view &str, const std::string_view &with);
bool ends_with(const std::string_view &str, char with);
std::string mid(std::string const &str, int start, int length = -1);
std::string replace_backslash_to_slash(std::string_view const &in);
std::string normalizePathSeparator(std::string const &str);

void dump(const uint8_t *ptr, size_t len);
#ifdef QT_VERSION
void dump(QByteArray const *in);
#endif
bool isText(std::string const &mimetype);
bool isImage(std::string const &mimetype);
bool isSVG(std::string const &mimetype);
bool isPSD(std::string const &mimetype);
bool isPDF(std::string const &mimetype);

std::string makeProxyServerURL(std::string text);
bool isExecutable(const std::string &cmd);
#ifdef QT_VERSION
bool isExecutable(QString const &cmd);
#endif

bool isValidMailAddress(std::string const &email);
#ifdef QT_VERSION
bool isValidMailAddress(const QString &email);
#endif

std::string_view trimmed(std::string_view const &s);
std::string_view trimQuotes(std::string_view s);
std::string_view trimNewLines(std::string_view s);

std::string bin_to_hex_string(const void *begin, const void *end);
std::vector<uint8_t> hex_string_to_bin(std::string_view const &s, const char *sep = nullptr);

int compare(uint8_t const *a, size_t n, uint8_t const *b, size_t m);
int compare(std::vector<uint8_t> const &a, std::vector<uint8_t> const &b);

std::string toLower(std::string_view const &s);
std::string toUpper(std::string_view const &s);

static inline std::vector<std::string> vector_string(std::vector<std::string_view> const &v)
{
	std::vector<std::string> out;
	for (auto const &s : v) {
		out.emplace_back(s);
	}
	return out;
}

template <typename T>
static inline T toi(std::string_view const &s, size_t *consumed = nullptr)
{
	T n = 0;
	size_t i = 0;
	bool sign = false;
	if (!s.empty()) {
		while (i < s.size() && isspace((unsigned char)s[i])) {
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

std::u16string convert_utf8_to_utf16(std::string_view const &s);
std::string convert_utf16_to_utf8(std::u16string_view const &s);

static inline void append(std::vector<char> *out, char const *ptr, size_t len)
{
	if (out && ptr && len > 0) {
		out->insert(out->end(), ptr, ptr + len);
	}
}

static inline void append(std::vector<char> *out, char c)
{
	if (out) {
		out->push_back(c);
	}
}

static inline void append(std::vector<char> *out, std::string_view const &v)
{
	append(out, v.data(), v.size());
}

std::string strip_vt(std::string_view const &s);

std::string getProgram(std::string const &cmdline);

} // namespace misc

#endif // MISC_H
