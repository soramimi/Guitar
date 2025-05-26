// String Formatter
// Copyright (C) 2025 S.Fuchita (soramimi_jp)
// This software is distributed under the MIT license.

#ifndef STRFORMAT_H
#define STRFORMAT_H

// #define STRFORMAT_NO_LOCALE
// #define STRFORMAT_NO_FP

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>
#include <vector>
#include <string_view>

#ifndef STRFORMAT_NO_LOCALE
#include <locale.h>
#endif

#ifdef _MSC_VER
#include <io.h>
#else
#include <unistd.h>
#endif

namespace strformat_ns {

class misc {
private:
	/**
	 * @brief Return 10 raised to an integer power.
	 *
	 * A small lookup table is used for the most common range to avoid
	 * calling the comparatively expensive `pow()` routine.  Values outside
	 * the table range fall back to `pow(10.0, exp)`.
	 *
	 * @param exp Decimal exponent (positive or negative).
	 * @return The value 10^exp as a double.
	 */
	static double pow10_int(int exp)
	{
		// Pre‑computed powers for |exp| ≤ 16
		static const double tbl[] = {
			1e+00, 1e+01, 1e+02, 1e+03, 1e+04, 1e+05, 1e+06,
			1e+07, 1e+08, 1e+09, 1e+10, 1e+11, 1e+12, 1e+13,
			1e+14, 1e+15, 1e+16
		};
		if (exp >= 0 && exp < static_cast<int>(sizeof tbl / sizeof *tbl))
			return tbl[exp];
		if (exp <= 0 && exp > -static_cast<int>(sizeof tbl / sizeof *tbl))
			return 1.0 / tbl[-exp];
		// Rare case: delegate to libm
		return std::pow(10.0, exp);
	}
public:
	/**
	 * @brief Locale‑independent `strtod` clone.
	 *
	 * Parses a floating‑point literal from a C‑string.  Leading white‑space,
	 * an optional sign, fractional part (with a mandatory '.' as the decimal
	 * separator), and an optional exponent (`e`/`E`) are recognised.
	 *
	 * The implementation **ignores the current locale**; the decimal point
	 * must be `'.'` and no thousands separators are accepted.
	 *
	 * @param nptr   Pointer to NUL‑terminated text to parse.
	 * @param endptr If non‑NULL, receives a pointer to the first character
	 *               following the parsed number (or `nptr` on failure).
	 * @return The parsed value.
	 */
	static double my_strtod(const char *nptr, char **endptr)
	{
		const char *s = nptr;
		bool sign = false;
		bool saw_digit = false;
		int frac_digits = 0;
		long exp_val = 0;
		bool exp_sign = false;
		double value = 0.0;

		// Skip leading white‑space
		while (std::isspace((unsigned char)*s)) ++s;

		// Parse optional sign
		if (*s == '+' || *s == '-') {
			if (*s == '-') sign = true;
			s++;
		}

		// Integer part
		while (std::isdigit((unsigned char)*s)) {
			saw_digit = true;
			value = value * 10.0 + (*s - '0');
			s++;
		}

		// Fractional part
		if (*s == '.') {
			s++;
			while (std::isdigit((unsigned char)*s)) {
				saw_digit = true;
				value = value * 10.0 + (*s - '0');
				s++;
				frac_digits++;
			}
		}

		// No digits at all -> conversion failure
		if (!saw_digit) {
			if (endptr) *endptr = const_cast<char *>(nptr);
			return 0.0;
		}

		// Exponent part
		if (*s == 'e' || *s == 'E') {
			s++;
			const char *exp_start = s;
			if (*s == '+' || *s == '-') {
				if (*s == '-') exp_sign = true;
				s++;
			}
			if (std::isdigit((unsigned char)*s)) {
				while (std::isdigit((unsigned char)*s)) {
					exp_val = exp_val * 10 + (*s - '0');
					s++;
				}
				if (exp_sign) {
					exp_val = -exp_val;
				}
			} else {
				// Roll back if 'e' is not followed by a valid exponent
				s = exp_start - 1;
			}
		}

		// Scale by 10^(exponent − #fractional‑digits)
		int total_exp = exp_val - frac_digits;
		if (total_exp != 0) {
			value *= pow10_int(total_exp);
		}

		// Apply sign
		if (sign) {
			value = -value;
		}

		// Set errno on overflow/underflow
		if (!std::isfinite(value)) {
			// errno = ERANGE;
			value = sign ? -HUGE_VAL : HUGE_VAL;
		} else if (value == 0.0 && saw_digit && total_exp != 0) {
			// errno = ERANGE;  // underflow
		}

		// Report where parsing stopped
		if (endptr) *endptr = const_cast<char *>(s);
		return value;
	}
};

struct NumberParser {
	char const *p;
	bool sign = false;
	int radix = 10;
	NumberParser(char const *ptr)
		: p(ptr)
	{
		while (isspace((unsigned char)*p)) {
			p++;
		}
		if (*p == '+') {
			p++;
		} else if (*p == '-') {
			sign = true;
			p++;
		}
		if (p[0] == '0') {
			if (p[1] == 'x' || p[1] == 'X') {
				p += 2;
				radix = 16;
			} else {
				int i = 1;
				while (p[i]) {
					int c = (unsigned char)p[i];
					if (c < '0' || c > '7') break;
					radix = 8;
					i++;
				}
			}
		}
	}
};

template <typename T> static inline T parse_number(char const *ptr, std::function<T(char const *p, int radix)> conv)
{
	NumberParser t(ptr);
	T v = conv(t.p, t.radix);
	if (t.sign) v = -v;
	return v;
}

struct Option_ {
#ifdef STRFORMAT_NO_LOCALE
	void *lc = nullptr;
#else
	struct lconv *lc = nullptr;
#endif
};

template <typename T> static inline T num(char const *value, Option_ const &opt);
template <> inline char num<char>(char const *value, Option_ const &opt)
{
	return parse_number<char>(value, [](char const *p, int radix){
		return (char)strtol(p, nullptr, radix);
	});
}
template <> inline int32_t num<int32_t>(char const *value, Option_ const &opt)
{
	return parse_number<int32_t>(value, [](char const *p, int radix){
		return strtol(p, nullptr, radix);
	});
}
template <> inline uint32_t num<uint32_t>(char const *value, Option_ const &opt)
{
	return parse_number<uint32_t>(value, [](char const *p, int radix){
		return strtoul(p, nullptr, radix);
	});
}
template <> inline int64_t num<int64_t>(char const *value, Option_ const &opt)
{
	return parse_number<int64_t>(value, [](char const *p, int radix){
		return strtoll(p, nullptr, radix);
	});
}
template <> inline uint64_t num<uint64_t>(char const *value, Option_ const &opt)
{
	return parse_number<uint64_t>(value, [](char const *p, int radix){
		return strtoull(p, nullptr, radix);
	});
}
#ifndef STRFORMAT_NO_FP
template <> inline double num<double>(char const *value, Option_ const &opt)
{
	return parse_number<double>(value, [&opt](char const *p, int radix){
		if (radix == 10) {
			if (opt.lc) {
				// locale-dependent
				return strtod(p, nullptr);
			} else {
				// locale-independent
				return misc::my_strtod(p, nullptr);
			}
		} else {
			return (double)strtoll(p, nullptr, radix);
		}
	});
}
#endif
template <typename T> static inline T num(std::string const &value, Option_ const &opt)
{
	return num<T>(value.data(), opt);
}

class string_formatter {
public:
	enum Flags {
		Locale = 0x0001,
	};
private:
	struct Part {
		Part *next;
		int size;
		char data[1];
	};
	struct PartList {
		Part *head = nullptr;
		Part *last = nullptr;
	};
	static Part *alloc_part(const char *data, int size)
	{
		Part *p = (Part *)new char[sizeof(Part) + size];
		p->next = nullptr;
		p->size = size;
		memcpy(p->data, data, size);
		p->data[size] = 0;
		return p;
	}
	static Part *alloc_part(const char *begin, const char *end)
	{
		return alloc_part(begin, end - begin);
	}
	static Part *alloc_part(const char *str)
	{
		return alloc_part(str, strlen(str));
	}
	static Part *alloc_part(const std::string_view &str)
	{
		return alloc_part(str.data(), (int)str.size());
	}
	static void free_part(Part **p)
	{
		if (p && *p) {
			delete[] *p;
			*p = nullptr;
		}
	}
	static void add_part(PartList *list, Part *part)
	{
		if (part) {
			if (!list->head) {
				list->head = part;
			}
			if (list->last) {
				list->last->next = part;
			}
			list->last = part;
		}
	}
	static void free_list(PartList *list)
	{
		Part *p = list->head;
		while (p) {
			Part *next = p->next;
			free_part(&p);
			p = next;
		}
		list->head = nullptr;
		list->last = nullptr;
	}
	static void add_chars(PartList *list, char c, int n)
	{
		Part *p = (Part *)new char[sizeof(Part) + n];
		p->next = nullptr;
		p->size = n;
		memset(p->data, c, n);
		p->data[n] = 0;
		add_part(list, p);
	}
	//
	static char const *digits_lower()
	{
		return "0123456789abcdef";
	}
	static char const *digits_upper()
	{
		return "0123456789ABCDEF";
	}
	//
#ifndef STRFORMAT_NO_FP
	Part *format_double(double val, int precision, bool trim_zeros, bool plus)
	{
		if (std::isnan(val)) return alloc_part("#NAN");
		if (std::isinf(val)) return alloc_part("#INF");

		char *ptr, *end;

		char *dot = nullptr;

		bool sign = val < 0;
		if (sign) {
			val = -val;
		}

		double intval = floor(val);
		val -= intval;

		int intlen = 0;
		if (intval == 0) {
			ptr = end = (char *)alloca(precision + 10) + 5;
		} else {
			double t = intval;
			do {
				t = floor(t / 10);
				intlen++;
			} while (t != 0);
			ptr = end = (char *)alloca(intlen + precision + 10) + intlen + 5;
		}

		if (precision > 0) {
			dot = end;
			*end++ = decimal_point();
			double v = val;
			int e = 0;
			while (v > 0 && v < 1) {
				v *= 10;
				e++;
			}
			while (v >= 1) {
				v /= 10;
				e--;
			}
			double add = 0.5;
			for (int i = 0; i < precision - e; i++) {
				add /= 10;
			}
			v += add;
			double t = floor(v);
			intval += t;
			v -= t;
			int i = 0;
			int n = intlen;
			int r = std::min(e, precision);
			while (i < r) {
				*end++ = '0';
				if (n != 0) {
					n++;
				}
				i++;
			}
			while (i < precision) {
				if (n < 16) {
					v *= 10;
					double m = floor(v);
					v -= m;
					*end++ = (char)m + '0';
				} else {
					*end++ = '0';
				}
				n++;
				i++;
			}
		} else {
			intval += floor(val + 0.5);
		}

		intlen = 0;
		double t = intval;
		do {
			t = floor(t / 10);
			intlen++;
		} while (t != 0);

		if (intval == 0) {
			*--ptr = '0';
		} else {
			double t = intval;
			for (int i = 0; i < intlen; i++) {
				t /= 10;
				double u = floor(t);
				*--ptr = (char)((t - u) * 10 + 0.49) + '0';
				t = u;
			}
		}

		if (sign) {
			*--ptr = '-';
		} else if (plus) {
			*--ptr = '+';
		}

		if (trim_zeros && dot) {
			while (dot < end) {
				char c = end[-1];
				if (c == '.') {
					end--;
					break;
				}
				if (c != '0') {
					break;
				}
				end--;
			}
		}

		return alloc_part(ptr, end);
	}
#endif
	static Part *format_int32(int32_t val, bool force_sign)
	{
		int n = 30;
		char *end = (char *)alloca(n) + n - 1;
		char *ptr = end;
		*end = 0;

		if (val == 0) {
			*--ptr = '0';
		} else {
			bool sign = (val < 0);
			uint32_t v;
			if (sign) {
				v = (uint32_t)-val;
			} else {
				v = (uint32_t)val;;
			}
			while (v != 0) {
				int c = v % 10 + '0';
				v /= 10;
				*--ptr = c;
			}
			if (sign) {
				*--ptr = '-';
			} else if (force_sign) {
				*--ptr = '+';
			}
		}

		return alloc_part(ptr, end);
	}
	static Part *format_uint32(uint32_t val)
	{
		int n = 30;
		char *end = (char *)alloca(n) + n - 1;
		char *ptr = end;
		*end = 0;

		if (val == 0) {
			*--ptr = '0';
		} else {
			while (val != 0) {
				int c = val % 10 + '0';
				val /= 10;
				*--ptr = c;
			}
		}

		return alloc_part(ptr, end);
	}
	static Part *format_int64(int64_t val, bool force_sign)
	{
		int n = 30;
		char *end = (char *)alloca(n) + n - 1;
		char *ptr = end;
		*end = 0;

		if (val == 0) {
			*--ptr = '0';
		} else {
			if (val == (int64_t)1 << 63) {
				*--ptr = '8';
				val /= 10;
			}
			bool sign = (val < 0);
			if (sign) val = -val;

			while (val != 0) {
				int c = val % 10 + '0';
				val /= 10;
				*--ptr = c;
			}
			if (sign) {
				*--ptr = '-';
			} else if (force_sign) {
				*--ptr = '+';
			}
		}

		return alloc_part(ptr, end);
	}
	static Part *format_uint64(uint64_t val)
	{
		int n = 30;
		char *end = (char *)alloca(n) + n - 1;
		char *ptr = end;
		*end = 0;

		if (val == 0) {
			*--ptr = '0';
		} else {
			while (val != 0) {
				int c = val % 10 + '0';
				val /= 10;
				*--ptr = c;
			}
		}

		return alloc_part(ptr, end);
	}
	static Part *format_oct32(uint32_t val)
	{
		int n = 30;
		char *end = (char *)alloca(n) + n - 1;
		char *ptr = end;
		*end = 0;

		char const *digits = digits_lower();

		if (val == 0) {
			*--ptr = '0';
		} else {
			while (val != 0) {
				char c = digits[val & 7];
				val >>= 3;
				*--ptr = c;
			}
		}

		return alloc_part(ptr, end);
	}
	static Part *format_oct64(uint64_t val)
	{
		int n = 30;
		char *end = (char *)alloca(n) + n - 1;
		char *ptr = end;
		*end = 0;

		char const *digits = digits_lower();

		if (val == 0) {
			*--ptr = '0';
		} else {
			while (val != 0) {
				char c = digits[val & 7];
				val >>= 3;
				*--ptr = c;
			}
		}

		return alloc_part(ptr, end);
	}
	static Part *format_hex32(uint32_t val, bool upper)
	{
		int n = 30;
		char *end = (char *)alloca(n) + n - 1;
		char *ptr = end;
		*end = 0;

		char const *digits = upper ? digits_upper() : digits_lower();

		if (val == 0) {
			*--ptr = '0';
		} else {
			while (val != 0) {
				char c = digits[val & 15];
				val >>= 4;
				*--ptr = c;
			}
		}

		return alloc_part(ptr, end);
	}
	static Part *format_hex64(uint64_t val, bool upper)
	{
		int n = 30;
		char *end = (char *)alloca(n) + n - 1;
		char *ptr = end;
		*end = 0;

		char const *digits = upper ? digits_upper() : digits_lower();

		if (val == 0) {
			*--ptr = '0';
		} else {
			while (val != 0) {
				char c = digits[val & 15];
				val >>= 4;
				*--ptr = c;
			}
		}

		return alloc_part(ptr, end);
	}
	static Part *format_pointer(void *val)
	{
		int n = sizeof(uintptr_t) * 2 + 1;
		char *end = (char *)alloca(n) + n - 1;
		char *ptr = end;
		*end = 0;

		char const *digits = digits_upper();

		uintptr_t v = (uintptr_t)val;
		for (int i = 0; i < (int)sizeof(uintptr_t) * 2; i++) {
			char c = digits[v & 15];
			v >>= 4;
			*--ptr = c;
		}

		return alloc_part(ptr, end);
	}
private:
	struct Private {
		std::string text;
		char const *head;
		char const *next;
		PartList list;
		bool upper : 1;
		bool zero_padding : 1;
		bool align_left : 1;
		bool plus : 1;
		int width;
		int precision;
		int lflag;
		Option_ opt;
	} q;

	void _init()
	{
		q.list = {};
	}

	void clear()
	{
		free_list(&q.list);
	}
	bool advance(bool complete)
	{
		bool r = false;
		auto Flush = [&](){
			if (q.head < q.next) {
				Part *p = alloc_part(q.head, q.next);
				add_part(&q.list, p);
				q.head = q.next;
			}
		};
		while (*q.next) {
			if (*q.next == '%') {
				if (q.next[1] == '%') {
					q.next++;
					Flush();
					q.next++;
					q.head = q.next;
				} else if (complete) {
					q.next++;
				} else {
					r = true;
					break;
				}
			} else {
				q.next++;
			}
		}
		Flush();
		return r;
	}
#ifndef STRFORMAT_NO_FP
	Part *format_f(double value, bool trim_zeros)
	{
		int pr = q.precision < 0 ? 6 : q.precision;
		return format_double(value, pr, trim_zeros, q.plus);
	}
#endif
	Part *format_c(char c)
	{
		return alloc_part(&c, &c + 1);
	}
	Part *format_o32(uint32_t value, int hint)
	{
		if (hint) {
			switch (hint) {
			case 'c': return format_c((char)value);
			case 'd': return format((int32_t)value, 0);
			case 'u': return format(value, 0);
			case 'x': return format_x32(value, 0);
#ifndef STRFORMAT_NO_FP
			case 'f': return format((double)value, 0);
#endif
			}
		}
		return format_oct32(value);
	}
	Part *format_o64(uint64_t value, int hint)
	{
		if (hint) {
			switch (hint) {
			case 'c': return format_c((char)value);
			case 'd': return format((int64_t)value, 0);
			case 'u': return format(value, 0);
			case 'x': return format_x64(value, 0);
#ifndef STRFORMAT_NO_FP
			case 'f': return format((double)value, 0);
#endif
			}
		}
		return format_oct64(value);
	}
	Part *format_x32(uint32_t value, int hint)
	{
		if (hint) {
			switch (hint) {
			case 'c': return format_c((char)value);
			case 'd': return format((int32_t)value, 0);
			case 'u': return format(value, 0);
			case 'o': return format_o32(value, 0);
#ifndef STRFORMAT_NO_FP
			case 'f': return format((double)value, 0);
#endif
			}
		}
		return format_hex32(value, q.upper);
	}
	Part *format_x64(uint64_t value, int hint)
	{
		if (hint) {
			switch (hint) {
			case 'c': return format_c((char)value);
			case 'd': return format((int64_t)value, 0);
			case 'u': return format(value, 0);
			case 'o': return format_o64(value, 0);
#ifndef STRFORMAT_NO_FP
			case 'f': return format((double)value, 0);
#endif
			}
		}
		return format_hex64(value, q.upper);
	}
	Part *format(char c, int hint)
	{
		return format((int32_t)c, hint);
	}
#ifndef STRFORMAT_NO_FP
	Part *format(double value, int hint)
	{
		if (hint) {
			switch (hint) {
			case 'c': return format_c((char)value);
			case 'd': return format((int64_t)value, 0);
			case 'u': return format((uint64_t)value, 0);
			case 'o': return format_o64((uint64_t)value, 0);
			case 'x': return format_x64((uint64_t)value, 0);
			case 's': return format_f(value, true);
			}
		}
		return format_f(value, false);
	}
#endif
	Part *format(int32_t value, int hint)
	{
		if (hint) {
			switch (hint) {
			case 'c': return format_c((char)value);
			case 'u': return format((uint32_t)value, 0);
			case 'o': return format_o32((uint32_t)value, 0);
			case 'x': return format_x32((uint32_t)value, 0);
#ifndef STRFORMAT_NO_FP
			case 'f': return format((double)value, 0);
#endif
			}
		}
		return format_int32(value, q.plus);
	}
	Part *format(uint32_t value, int hint)
	{
		if (hint) {
			switch (hint) {
			case 'c': return format_c((char)value);
			case 'd': return format((int32_t)value, 0);
			case 'o': return format_o32((uint32_t)value, 0);
			case 'x': return format_x32((uint32_t)value, 0);
#ifndef STRFORMAT_NO_FP
			case 'f': return format((double)value, 0);
#endif
			}
		}
		return format_uint32(value);
	}
	Part *format(int64_t value, int hint)
	{
		if (hint) {
			switch (hint) {
			case 'c': return format_c((char)value);
			case 'u': return format((uint64_t)value, 0);
			case 'o': return format_o64((uint64_t)value, 0);
			case 'x': return format_x64((uint64_t)value, 0);
#ifndef STRFORMAT_NO_FP
			case 'f': return format((double)value, 0);
#endif
			}
		}
		return format_int64(value, q.plus);
	}
	Part *format(uint64_t value, int hint)
	{
		if (hint) {
			switch (hint) {
			case 'c': return format_c((char)value);
			case 'd': return format((int64_t)value, 0);
			case 'o': return format_oct64(value);
			case 'x': return format_hex64(value, false);
#ifndef STRFORMAT_NO_FP
			case 'f': return format((double)value, 0);
#endif
			}
		}
		return format_uint64(value);
	}
	Part *format(char const *value, int hint)
	{
		if (!value) {
			return alloc_part("(null)");
		}
		if (hint) {
			switch (hint) {
			case 'c':
				return format_c(num<char>(value, q.opt));
			case 'd':
				if (q.lflag == 0) {
					return format(num<int32_t>(value, q.opt), 0);
				} else {
					return format(num<int64_t>(value, q.opt), 0);
				}
			case 'u': case 'o': case 'x':
				if (q.lflag == 0) {
					return format(num<uint32_t>(value, q.opt), hint);
				} else {
					return format(num<uint64_t>(value, q.opt), hint);
				}
#ifndef STRFORMAT_NO_FP
			case 'f':
				return format(num<double>(value, q.opt), hint);
#endif
			}
		}
		return alloc_part(value, value + strlen(value));
	}
	Part *format(std::string_view const &value, int hint)
	{
		if (hint == 's') {
			return alloc_part(value);
		}
		return format(value.data(), hint);
	}
	Part *format(std::vector<char> const &value, int hint)
	{
		std::string_view sv(value.data(), value.size());
		if (hint == 's') {
			return alloc_part(sv);
		}
		return format(sv, hint);
	}
	Part *format_p(void *val)
	{
		return format_pointer(val);
	}
	void reset_format_params()
	{
		q.upper = false;
		q.zero_padding = false;
		q.align_left = false;
		q.plus = false;
		q.width = -1;
		q.precision = -1;
		q.lflag = 0;
	}
	void format(std::function<Part *(int)> const &callback, int width, int precision)
	{
		if (advance(false)) {
			if (*q.next == '%') {
				q.next++;
			}

			reset_format_params();

			while (1) {
				int c = (unsigned char)*q.next;
				if (c == '0') {
					q.zero_padding = true;
				} else if (c == '+') {
					q.plus = true;
				} else if (c == '-') {
					q.align_left = true;
				} else {
					break;
				}
				q.next++;
			}

			auto GetNumber = [&](int alternate_value){
				int value = -1;
				if (*q.next == '*') {
					q.next++;
				} else {
					while (1) {
						int c = (unsigned char)*q.next;
						if (!isdigit(c)) break;
						if (value < 0) {
							value = 0;
						} else {
							value *= 10;
						}
						value += c - '0';
						q.next++;
					}
				}
				if (value < 0) {
					value = alternate_value;
				}
				return value;
			};

			q.width = GetNumber(width);

			if (*q.next == '.') {
				q.next++;
			}

			q.precision = GetNumber(precision);

			while (*q.next == 'l') {
				q.lflag++;
				q.next++;
			}

			Part *p = nullptr;

			int c = (unsigned char)*q.next;
			if (isupper(c)) {
				q.upper = true;
				c = tolower(c);
			}
			if (isalpha(c)) {
				p = callback(c);
				q.next++;
			}
			if (p) {
				int padlen = q.width - p->size;
				if (padlen > 0 && !q.align_left) {
					if (q.zero_padding) {
						char c = p->data[0];
						add_chars(&q.list, '0', padlen);
						if (c == '+' || c == '-') {
							q.list.last->data[0] = c;
							p->data[0] = '0';
						}
					} else {
						add_chars(&q.list, ' ', padlen);
					}
				}

				add_part(&q.list, p);

				if (padlen > 0 && q.align_left) {
					add_chars(&q.list, ' ', padlen);
				}
			}

			q.head = q.next;
		}
	}
	int length()
	{
		advance(true);
		int len = 0;
		for (Part *p = q.list.head; p; p = p->next) {
			len += p->size;
		}
		return len;
	}
#ifndef STRFORMAT_NO_LOCALE
	void use_locale(bool use)
	{
		if (use) {
			q.opt.lc = localeconv();
		} else {
			q.opt.lc = nullptr;
		}
	}
#endif
	void set_flags(int flags)
	{
#ifndef STRFORMAT_NO_LOCALE
		use_locale(flags & Locale);
#endif
	}
public:
	string_formatter(string_formatter const &) = delete;
	void operator = (string_formatter const &) = delete;

	string_formatter(string_formatter &&r)
	{
		q = r.q;
		r._init();
	}
	void operator = (string_formatter &&r)
	{
		clear();
		q = r.q;
		r._init();
	}

	string_formatter(int flags = 0, std::string const &text = {})
	{
		reset(flags, text);
	}

	string_formatter(std::string const &text)
	{
		reset(0, text);
	}
	~string_formatter()
	{
		clear();
	}

	char decimal_point() const
	{
#ifndef STRFORMAT_NO_LOCALE
		if (q.opt.lc && q.opt.lc->decimal_point) {
			return *q.opt.lc->decimal_point;
		}
#endif
		return '.';
	}

	string_formatter &reset(int flags, std::string const &text)
	{
		clear();
		q.text = text;
		q.head = q.text.data();
		q.next = q.head;

#ifndef STRFORMAT_NO_LOCALE
		use_locale(flags & Locale);
#endif

		return *this;
	}

	string_formatter &append(std::string const &s)
	{
		q.text += s;
		return *this;
	}
	string_formatter &append(char const *s)
	{
		q.text += s;
		return *this;
	}

	template <typename T> string_formatter &arg(T const &value, int width = -1, int precision = -1)
	{
		format([&](int hint){ return format(value, hint); }, width, precision);
		return *this;
	}
#ifndef STRFORMAT_NO_FP
	string_formatter &f(double value, int width = -1, int precision = -1)
	{
		return arg(value, width, precision);
	}
#endif
	string_formatter &c(char value, int width = -1, int precision = -1)
	{
		return arg(value, width, precision);
	}
	string_formatter &d(int32_t value, int width = -1, int precision = -1)
	{
		return arg(value, width, precision);
	}
	string_formatter &ld(int64_t value, int width = -1, int precision = -1)
	{
		return arg(value, width, precision);
	}
	string_formatter &u(uint32_t value, int width = -1, int precision = -1)
	{
		return arg(value, width, precision);
	}
	string_formatter &lu(uint64_t value, int width = -1, int precision = -1)
	{
		return arg(value, width, precision);
	}
	string_formatter &o(int32_t value, int width = -1, int precision = -1)
	{
		format([&](int hint){ return format_o32(value, hint); }, width, precision);
		return *this;
	}
	string_formatter &lo(int64_t value, int width = -1, int precision = -1)
	{
		format([&](int hint){ return format_o64(value, hint); }, width, precision);
		return *this;
	}
	string_formatter &x(int32_t value, int width = -1, int precision = -1)
	{
		format([&](int hint){ return format_x32(value, hint); }, width, precision);
		return *this;
	}
	string_formatter &lx(int64_t value, int width = -1, int precision = -1)
	{
		format([&](int hint){ return format_x64(value, hint); }, width, precision);
		return *this;
	}
	string_formatter &s(char const *value, int width = -1, int precision = -1)
	{
		return arg(value, width, precision);
	}
	string_formatter &s(std::string_view const &value, int width = -1, int precision = -1)
	{
		return arg(value, width, precision);
	}
	string_formatter &p(void *value, int width = -1, int precision = -1)
	{
		format([&](int hint){ (void)hint; return format_p(value); }, width, precision);
		return *this;
	}

	template <typename T> string_formatter &operator () (T const &value, int width = -1, int precision = -1)
	{
		return arg(value, width, precision);
	}
	void render(std::function<void (char const *ptr, int len)> const &to)
	{
		advance(true);
		for (Part *p = q.list.head; p; p = p->next) {
			to(p->data, p->size);
		}
	}
	void vec(std::vector<char> *vec)
	{
		vec->reserve(vec->size() + length());
		render([&](char const *ptr, int len){
			vec->insert(vec->end(), ptr, ptr + len);
		});
	}
	void write_to(FILE *fp)
	{
		render([&](char const *ptr, int len){
			fwrite(ptr, 1, len, fp);
		});
	}
	void write_to(int fd)
	{
		render([&](char const *ptr, int len){
			::write(fd, ptr, len);
		});
	}
	void put()
	{
		write_to(stdout);
	}
	void err()
	{
		write_to(stderr);
	}
	std::string str()
	{
		int n = length();
		char *p;
		std::vector<char> tmp;
		if (n < 4096) { // if smaller, use stack
			p = (char *)alloca(n);
		} else {
			tmp.reserve(n); // if larger, use heap
			p = tmp.data();
		}
		char *d = p;
		render([&](char const *ptr, int len){
			memcpy(d, ptr, len);
			d += len;
		});
		return std::string(p, n);
	}
	operator std::string ()
	{
		return str();
	}
};

} // namespace strformat_ns

// using strformat = strformat_ns::string_formatter;
using strf = strformat_ns::string_formatter;

#endif // STRFORMAT_H
