// String Formatter
// Copyright (C) 2020 S.Fuchita (soramimi_jp)
// This software is distributed under the MIT license.

#ifndef STRFORMAT_H
#define STRFORMAT_H

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

#ifdef _MSC_VER
#include <io.h>
#else
#include <unistd.h>
#endif

namespace strformat_ns {

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
template <typename T> static inline T num(char const *value);
template <> inline char num<char>(char const *value)
{
	return parse_number<char>(value, [](char const *p, int radix){
		return (char)strtol(p, nullptr, radix);
	});
}
template <> inline int32_t num<int32_t>(char const *value)
{
	return parse_number<int32_t>(value, [](char const *p, int radix){
		return strtol(p, nullptr, radix);
	});
}
template <> inline uint32_t num<uint32_t>(char const *value)
{
	return parse_number<uint32_t>(value, [](char const *p, int radix){
		return strtoul(p, nullptr, radix);
	});
}
template <> inline int64_t num<int64_t>(char const *value)
{
	return parse_number<int64_t>(value, [](char const *p, int radix){
		return strtoll(p, nullptr, radix);
	});
}
template <> inline uint64_t num<uint64_t>(char const *value)
{
	return parse_number<uint64_t>(value, [](char const *p, int radix){
		return strtoull(p, nullptr, radix);
	});
}
#ifndef STRFORMAT_NO_FP
template <> inline double num<double>(char const *value)
{
	return parse_number<double>(value, [](char const *p, int radix){
		if (radix == 10) {
			return strtod(p, nullptr);
		} else {
			return (double)strtoll(p, nullptr, radix);
		}
	});
}
#endif
template <typename T> static inline T num(std::string const &value)
{
	return num<T>(value.data());
}

class string_formatter {
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
		Part *p = (Part *)malloc(sizeof(Part) + size);
		if (!p)
            return nullptr;
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
			free(*p);
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
		Part *p = (Part *)malloc(sizeof(Part) + n);
		if (!p)
            return;
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
	static Part *format_double(double val, int precision, bool trim_zeros, bool plus)
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
			*end++ = '.';
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
	std::string text_;
	char const *head_;
	char const *next_;
	PartList list_;
	bool upper_ : 1;
	bool zero_padding_ : 1;
	bool align_left_ : 1;
	bool plus_ : 1;
	int width_;
	int precision_;
	int lflag_;

	void clear()
	{
		free_list(&list_);
	}
	bool advance(bool complete)
	{
		bool r = false;
		auto Flush = [&](){
			if (head_ < next_) {
				Part *p = alloc_part(head_, next_);
				add_part(&list_, p);
				head_ = next_;
			}
		};
		while (*next_) {
			if (*next_ == '%') {
				if (next_[1] == '%') {
					next_++;
					Flush();
					next_++;
					head_ = next_;
				} else if (complete) {
					next_++;
				} else {
					r = true;
					break;
				}
			} else {
				next_++;
			}
		}
		Flush();
		return r;
	}
#ifndef STRFORMAT_NO_FP
	Part *format_f(double value, bool trim_zeros)
	{
		int pr = precision_ < 0 ? 6 : precision_;
		return format_double(value, pr, trim_zeros, plus_);
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
		return format_hex32(value, upper_);
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
		return format_hex64(value, upper_);
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
		return format_int32(value, plus_);
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
		return format_int64(value, plus_);
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
				return format_c(num<char>(value));
			case 'd':
				if (lflag_ == 0) {
					return format(num<int32_t>(value), 0);
				} else {
					return format(num<int64_t>(value), 0);
				}
			case 'u': case 'o': case 'x':
				if (lflag_ == 0) {
					return format(num<uint32_t>(value), hint);
				} else {
					return format(num<uint64_t>(value), hint);
				}
#ifndef STRFORMAT_NO_FP
			case 'f':
				return format(num<double>(value), hint);
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
	Part *format_p(void *val)
	{
		return format_pointer(val);
	}
	void reset_format_params()
	{
		upper_ = false;
		zero_padding_ = false;
		align_left_ = false;
		plus_ = false;
		width_ = -1;
		precision_ = -1;
		lflag_ = 0;
	}
	void format(std::function<Part *(int)> const &callback, int width, int precision)
	{
		if (advance(false)) {
			if (*next_ == '%') {
				next_++;
			}

			reset_format_params();

			while (1) {
				int c = (unsigned char)*next_;
				if (c == '0') {
					zero_padding_ = true;
				} else if (c == '+') {
					plus_ = true;
				} else if (c == '-') {
					align_left_ = true;
				} else {
					break;
				}
				next_++;
			}

			auto GetNumber = [&](int alternate_value){
				int value = -1;
				if (*next_ == '*') {
					next_++;
				} else {
					while (1) {
						int c = (unsigned char)*next_;
						if (!isdigit(c)) break;
						if (value < 0) {
							value = 0;
						} else {
							value *= 10;
						}
						value += c - '0';
						next_++;
					}
				}
				if (value < 0) {
					value = alternate_value;
				}
				return value;
			};

			width_ = GetNumber(width);

			if (*next_ == '.') {
				next_++;
			}

			precision_ = GetNumber(precision);

			while (*next_ == 'l') {
				lflag_++;
				next_++;
			}

			Part *p = nullptr;

			int c = (unsigned char)*next_;
			if (isupper(c)) {
				upper_ = true;
				c = tolower(c);
			}
			if (isalpha(c)) {
				p = callback(c);
				next_++;
			}
			if (p) {
				int padlen = width_ - p->size;
				if (padlen > 0 && !align_left_) {
					if (zero_padding_) {
						char c = p->data[0];
						add_chars(&list_, '0', padlen);
						if (c == '+' || c == '-') {
							list_.last->data[0] = c;
							p->data[0] = '0';
						}
					} else {
						add_chars(&list_, ' ', padlen);
					}
				}

				add_part(&list_, p);

				if (padlen > 0 && align_left_) {
					add_chars(&list_, ' ', padlen);
				}
			}

			head_ = next_;
		} else {
			reset_format_params();
			Part *p = callback('s');
			add_part(&list_, p);
		}
	}
	int length()
	{
		advance(true);
		int len = 0;
		for (Part *p = list_.head; p; p = p->next) {
			len += p->size;
		}
		return len;
	}
public:
	string_formatter(string_formatter &&) = delete;
	string_formatter(string_formatter const &) = delete;
	void operator = (string_formatter &&) = delete;
	void operator = (string_formatter const &) = delete;

	string_formatter()
	{
		reset();
	}

	string_formatter(std::string const &text)
		: text_(text)
	{
		reset();
	}
	~string_formatter()
	{
		clear();
	}

	string_formatter &reset()
	{
		clear();
		head_ = text_.data();
		next_ = head_;
		return *this;
	}

	string_formatter &append(std::string const &s)
	{
		text_ += s;
		return *this;
	}
	string_formatter &append(char const *s)
	{
		text_ += s;
		return *this;
	}

	template <typename T> string_formatter &a(T const &value, int width = -1, int precision = -1)
	{
		format([&](int hint){ return format(value, hint); }, width, precision);
		return *this;
	}
#ifndef STRFORMAT_NO_FP
	string_formatter &f(double value, int width = -1, int precision = -1)
	{
		return a(value, width, precision);
	}
#endif
	string_formatter &c(char value, int width = -1, int precision = -1)
	{
		return a(value, width, precision);
	}
	string_formatter &d(int32_t value, int width = -1, int precision = -1)
	{
		return a(value, width, precision);
	}
	string_formatter &ld(int64_t value, int width = -1, int precision = -1)
	{
		return a(value, width, precision);
	}
	string_formatter &u(uint32_t value, int width = -1, int precision = -1)
	{
		return a(value, width, precision);
	}
	string_formatter &lu(uint64_t value, int width = -1, int precision = -1)
	{
		return a(value, width, precision);
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
		return a(value, width, precision);
	}
	string_formatter &s(std::string_view const &value, int width = -1, int precision = -1)
	{
		return a(value, width, precision);
	}
	string_formatter &p(void *value, int width = -1, int precision = -1)
	{
		format([&](int hint){ (void)hint; return format_p(value); }, width, precision);
		return *this;
	}

	template <typename T> string_formatter &operator () (T const &value, int width = -1, int precision = -1)
	{
		return a(value, width, precision);
	}
	void render(std::function<void (char const *ptr, int len)> const &to)
	{
		advance(true);
		for (Part *p = list_.head; p; p = p->next) {
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
		if (n < 1) return {};
		char *p;
		std::vector<char> tmp;
		if (n < 0x20000) {
			p = (char *)alloca(n);
		} else {
			tmp.reserve(n);
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

using strformat = strformat_ns::string_formatter;

#endif // STRFORMAT_H
