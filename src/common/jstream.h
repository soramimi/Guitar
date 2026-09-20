// Jstream - Header-only Streaming pull-based JSON Parser and Generator
// Copyright (C) 2026 S.Fuchita (soramimi)
// This software is distributed under the MIT license.

#ifndef JSTREAM_H_
#define JSTREAM_H_

#include <cassert>
#include <cctype>
#include <charconv>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace jstream {

static constexpr char hex_table[] = "0123456789ABCDEF";

inline void hex_u8(uint8_t v, char *p)
{
	p[0] = hex_table[(v >> 4) & 0x0f];
	p[1] = hex_table[v & 0x0f];
}

inline void hex_u16(uint16_t v, char *p)
{
	hex_u8(v >> 8, p);
	hex_u8(v & 0xff, p + 2);
}

static std::vector<char> encode_json_string(std::string_view in)
{
	std::vector<char> ret;
	char const *ptr = in.data();
	char const *end = ptr + in.size();
	ret.reserve(static_cast<size_t>(end - ptr) + 10);
	while (ptr < end) {
		unsigned char c = static_cast<unsigned char>(*ptr);
		char const *next = ptr + 1;
		auto push_escape = [&](char esc) {
			ret.push_back('\\');
			ret.push_back(esc);
		};
		auto push_u00 = [&](unsigned char v) {
			char tmp[6];
			tmp[0] = '\\';
			tmp[1] = 'u';
			tmp[2] = '0';
			tmp[3] = '0';
			hex_u8(v, tmp + 4);
			ret.insert(ret.end(), tmp, tmp + 6);
		};
		if (c == '\"') {
			push_escape('\"');
		} else if (c == '\\') {
			push_escape('\\');
		} else if (c == '\b') {
			push_escape('b');
		} else if (c == '\f') {
			push_escape('f');
		} else if (c == '\n') {
			push_escape('n');
		} else if (c == '\r') {
			push_escape('r');
		} else if (c == '\t') {
			push_escape('t');
		} else if (c < 0x20) {
			push_u00(c);
		} else if (c < 0x7f) {
			ret.push_back(static_cast<char>(c));
		} else {
			int utf8len = 0;
			uint32_t unicode = 0;
			if ((c & 0xe0) == 0xc0 && next < end) {
				unsigned char d = static_cast<unsigned char>(*next);
				if ((d & 0xc0) == 0x80) {
					unicode = ((c & 0x1f) << 6) | (d & 0x3f);
					if (unicode >= 0x80) utf8len = 2;
				}
			} else if ((c & 0xf0) == 0xe0 && next + 1 < end) {
				unsigned char d = static_cast<unsigned char>(*next);
				unsigned char e = static_cast<unsigned char>(next[1]);
				if ((d & 0xc0) == 0x80 && (e & 0xc0) == 0x80) {
					unicode = ((c & 0x0f) << 12) | ((d & 0x3f) << 6) | (e & 0x3f);
					if (unicode >= 0x800 && unicode < 0x10000) utf8len = 3;
				}
			} else if ((c & 0xf8) == 0xf0 && next + 2 < end) {
				unsigned char d = static_cast<unsigned char>(*next);
				unsigned char e = static_cast<unsigned char>(next[1]);
				unsigned char f = static_cast<unsigned char>(next[2]);
				if ((d & 0xc0) == 0x80 && (e & 0xc0) == 0x80 && (f & 0xc0) == 0x80) {
					unicode = ((c & 0x07) << 18) | ((d & 0x3f) << 12) | ((e & 0x3f) << 6) | (f & 0x3f);
					if (unicode >= 0x10000 && unicode < 0x110000) utf8len = 4;
				}
			}
			if (utf8len > 0) {
				next = ptr + utf8len;
				ret.insert(ret.end(), ptr, next);
			} else {
				// Invalid UTF-8 byte: escape as \u00XX.
				push_u00(c);
			}
		}
		ptr = next;
	}
	return ret;
}

/**
 * @brief Return 10 raised to an integer power.
 *
 * A small lookup table is used for the most common range to avoid
 * calling the comparatively expensive `pow()` routine.  Values outside
 * the table range fall back to `pow(10.0, exp)`.
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
	while (std::isspace(static_cast<unsigned char>(*s))) ++s;
	
	// Parse optional sign
	if (*s == '+' || *s == '-') {
		if (*s == '-') sign = true;
		s++;
	}
	
	// Integer part
	while (std::isdigit(static_cast<unsigned char>(*s))) {
		saw_digit = true;
		value = value * 10.0 + (*s - '0');
		s++;
	}
	
	// Fractional part
	if (*s == '.') {
		s++;
		while (std::isdigit(static_cast<unsigned char>(*s))) {
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
		if (std::isdigit(static_cast<unsigned char>(*s))) {
			while (std::isdigit(static_cast<unsigned char>(*s))) {
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

static std::string format_double(double val, bool allow_nan)
{
	if (std::isnan(val)) {
		if (allow_nan) {
			return "NaN";
		}
		return {};
	}
	if (std::isinf(val)) {
		if (allow_nan) {
			return std::signbit(val) ? "-Infinity" : "Infinity";
		}
		return {};
	}
	
	// std::to_chars produces the shortest round-trip representation without locale dependency
	char buf[32];
	auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), val);
	if (ec != std::errc{}) {
		return {};
	}
	return std::string(buf, ptr);
}

enum StateType {
	// Symbols
	None = 0,
	Null,
	False,
	True,
	// States
	Key = 100,
	Comma,
	StartObject,
	EndObject,
	StartArray,
	EndArray,
	String,
	Number,
	//
	EndDocument,
};

class Reader {
public:
	struct Error {
		std::string what_;
		size_t offset = 0;
		size_t line = 0;
		size_t column = 0;
		std::string what() const { return what_; }
	};
private:
	bool is_streaming_input_mode() const
	{
		return (bool)d.fn_input_calback;
	}
	void need_input() const
	{
		if (is_streaming_input_mode()) {
			d.fn_input_calback();
		}
	}
	
	int peek_next_char() const
	{
		if (d.ptr && d.end && d.ptr < d.end) {
			return (unsigned char)*d.ptr;
		}
		need_input();
		if (d.ptr && d.end && d.ptr < d.end) {
			return (unsigned char)*d.ptr;
		}
		return -1;
	}
	
	int scan_space(char const *begin, char const *end)
	{
		char const *ptr = begin;
		while (ptr < end) {
			if (std::isspace(static_cast<unsigned char>(*ptr))) {
				ptr++;
				continue;
			}
			if (d.allow_comment && *ptr == '/' && ptr + 1 < end) {
				if (ptr[1] == '/') {
					ptr += 2;
					while (ptr < end && *ptr != '\r' && *ptr != '\n') {
						ptr++;
					}
					continue;
				}
				if (ptr[1] == '*') {
					ptr += 2;
					while (ptr + 1 < end) {
						if (*ptr == '*' && ptr[1] == '/') {
							ptr += 2;
							break;
						}
						ptr++;
					}
					continue;
				}
			}
			break;
		}
		return int(ptr - begin);
	}
	
	bool skip_space()
	{
		bool ret = false;
		while (1) {
			int c = peek_next_char();
			if (c < 0) {
				if (d.comment_state != 0) {
					d.not_enough_input = true;
				}
				break;
			}
			if (d.comment_state != 0) {
				if (d.comment_state == '*') {
					if (c == '/') {
						d.comment_state = 0;
					}
				} else if (d.comment_state == '/') {
					if (c == '\n' || c == '\r') {
						d.comment_state = 0;
					}
				}
			} else if (!std::isspace(c)) {
				if (d.allow_comment && c == '/') {
					if (d.ptr + 1 >= d.end) {
						need_input();
					}
					if (d.ptr + 1 < d.end) {
						char t = d.ptr[1];
						if (t == '*' || t == '/') {
							d.comment_state = t;
							d.ptr += 2;
							ret = true;
							continue;
						}
					}
				}
				break;
			}
			d.ptr++;
			ret = true;
		}
		return ret;
	}

	int parse_symbol(char const *begin, char const *end, std::string *out)
	{
		char const *ptr = begin;
		ptr += scan_space(ptr, end);
		char const *start = ptr;
		while (ptr < end) {
			if (!std::isalnum(static_cast<unsigned char>(*ptr)) && *ptr != '_') break;
			ptr++;
		}
		if (ptr > start) {
			*out = std::string(start, ptr);
			return int(ptr - begin);
		}
		out->clear();
		return 0;
	}

	static bool validate_json_number(char const *start, char const *end, char const **out_end)
	{
		char const *p = start;
		if (p < end && *p == '-') {
			p++;
		}
		if (p >= end) {
			return false;
		}
		if (*p == '0') {
			p++;
		} else if (*p >= '1' && *p <= '9') {
			p++;
			while (p < end && std::isdigit(static_cast<unsigned char>(*p))) {
				p++;
			}
		} else {
			return false;
		}
		if (p < end && *p == '.') {
			p++;
			if (p >= end || !std::isdigit(static_cast<unsigned char>(*p))) {
				return false;
			}
			while (p < end && std::isdigit(static_cast<unsigned char>(*p))) {
				p++;
			}
		}
		if (p < end && (*p == 'e' || *p == 'E')) {
			p++;
			if (p < end && (*p == '+' || *p == '-')) {
				p++;
			}
			if (p >= end || !std::isdigit(static_cast<unsigned char>(*p))) {
				return false;
			}
			while (p < end && std::isdigit(static_cast<unsigned char>(*p))) {
				p++;
			}
		}
		*out_end = p;
		return true;
	}

	int parse_number(char const *begin, char const *end, double *out)
	{
		*out = 0;
		char const *ptr = begin;
		ptr += scan_space(ptr, end);

		if (d.allow_hexadecimal) {
			char const *p = ptr;
			bool sign = false;
			if (p + 1 < end && *p == '-') {
				p++;
				sign = true;
			}
			if (p + 1 < end && *p == '0' && (p[1] == 'x' || p[1] == 'X')) {
				p += 2;
				char const *digits = p;
				while (p < end && std::isxdigit(static_cast<unsigned char>(*p))) {
					p++;
				}
				if (p > digits) {
					long long v = 0;
					for (char const *q = digits; q < p; ++q) {
						unsigned char c = static_cast<unsigned char>(*q);
						int digit = 0;
						if (c >= '0' && c <= '9') digit = c - '0';
						else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
						else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
						if (v > (LLONG_MAX - digit) / 16) {
							push_error("hexadecimal integer overflow");
							return 0;
						}
						v = v * 16 + digit;
					}
					*out = double(sign ? -v : v);
					return int(p - begin);
				}
			}
		}

		if (d.allow_special_constant) {
			char const *p = ptr;
			bool sign = false;
			if (p < end && *p == '-') {
				p++;
				sign = true;
			}
			char const *word = p;
			while (p < end && std::isalpha(static_cast<unsigned char>(*p))) {
				p++;
			}
			size_t len = p - word;
			if (len == 8 && std::strncmp(word, "Infinity", 8) == 0) {
				*out = sign ? -INFINITY : INFINITY;
				return int(p - begin);
			}
			if (len == 3 && std::strncmp(word, "NaN", 3) == 0) {
				*out = NAN;
				return int(p - begin);
			}
		}

		char const *start = ptr;
		char const *num_end = nullptr;
		if (!validate_json_number(start, end, &num_end)) {
			return 0;
		}
		ptr = num_end;

#if defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L
		// std::from_chars for double is available: parse without copying.
		auto [p2, ec] = std::from_chars(start, ptr, *out);
		if (ec == std::errc{}) {
			(void)p2;
			return int(ptr - begin);
		}
#endif

		// Fallback: copy to a null-terminated buffer for locale-independent strtod.
		std::vector<char> vec(start, ptr);
		vec.push_back(0);
		*out = my_strtod(vec.data(), nullptr);

		return int(ptr - begin);
	}

	int parse_string(char const *begin, char const *end, std::string *out)
	{
		char const *ptr = begin;
		ptr += scan_space(ptr, end);
		if (ptr < end && *ptr == '\"') {
			ptr++;
			std::string s;
			s.reserve(static_cast<size_t>(end - ptr));
			while (ptr < end) {
				if (*ptr == '\"') {
					*out = std::move(s);
					ptr++;
					return int(ptr - begin);
				} else if (*ptr == '\\') {
					ptr++;
					if (ptr >= end) break;
					char c = *ptr;
					switch (c) {
					case 'b': s.push_back('\b'); ptr++; break;
					case 'n': s.push_back('\n'); ptr++; break;
					case 'r': s.push_back('\r'); ptr++; break;
					case 'f': s.push_back('\f'); ptr++; break;
					case 't': s.push_back('\t'); ptr++; break;
					case 'v': s.push_back('\v'); ptr++; break;
					case '\\':
					case '\"':
						s.push_back(c); ptr++;
						break;
					case 'u':
					{
						ptr++;
						auto parse_hex4 = [&](char const *p, uint32_t *out_unicode) -> int {
							if (p + 4 > end) return 0;
							for (int i = 0; i < 4; i++) {
								if (!std::isxdigit(static_cast<unsigned char>(p[i]))) return 0;
							}
							char tmp[5] = { p[0], p[1], p[2], p[3], 0 };
							*out_unicode = static_cast<uint32_t>(std::strtol(tmp, nullptr, 16));
							return 4;
						};
						uint32_t unicode = 0;
						int n = parse_hex4(ptr, &unicode);
						if (n == 0) {
							push_error("invalid unicode escape");
							return 0;
						}
						ptr += n;
						if (unicode >= 0xd800 && unicode < 0xdc00) {
							uint32_t surrogate = 0;
							if (ptr + 5 < end && ptr[0] == '\\' && ptr[1] == 'u') {
								int n2 = parse_hex4(ptr + 2, &surrogate);
								if (n2 == 4 && surrogate >= 0xdc00 && surrogate < 0xe000) {
									ptr += 6;
									unicode = ((unicode - 0xd800) << 10) + (surrogate - 0xdc00) + 0x10000;
								} else {
									push_error("invalid surrogate pair");
									return 0;
								}
							} else {
								push_error("unpaired high surrogate");
								return 0;
							}
						} else if (unicode >= 0xdc00 && unicode < 0xe000) {
							push_error("unpaired low surrogate");
							return 0;
						}
						if (unicode >= 0x110000) {
							push_error("invalid unicode code point");
							return 0;
						}
						if (unicode < 0x80) {
							s.push_back(static_cast<char>(unicode));
						} else if (unicode < 0x800) {
							s.push_back(static_cast<char>(((unicode >> 6) & 0x1f) | 0xc0));
							s.push_back(static_cast<char>((unicode & 0x3f) | 0x80));
						} else if (unicode < 0x10000) {
							s.push_back(static_cast<char>(((unicode >> 12) & 0x0f) | 0xe0));
							s.push_back(static_cast<char>(((unicode >> 6) & 0x3f) | 0x80));
							s.push_back(static_cast<char>((unicode & 0x3f) | 0x80));
						} else {
							s.push_back(static_cast<char>(((unicode >> 18) & 0x07) | 0xf0));
							s.push_back(static_cast<char>(((unicode >> 12) & 0x3f) | 0x80));
							s.push_back(static_cast<char>(((unicode >> 6) & 0x3f) | 0x80));
							s.push_back(static_cast<char>((unicode & 0x3f) | 0x80));
						}
						break;
					}
					default:
						s.push_back(c);
						ptr++;
						break;
					}
				} else {
					s.push_back(*ptr);
					ptr++;
				}
			}
		}
		return 0; // unexpected end of string
	}
private:
	struct StateItem {
		StateType type = None;
		char const *ptr = nullptr;
		StateItem() = default;
		StateItem(StateType type, char const *ptr = nullptr)
			: type(type)
			, ptr(ptr)
		{
		}
	};
	struct ParserData {
		bool not_enough_input = false;
		std::optional<std::vector<char>> input_buffer;
		std::function<void ()> fn_input_calback;
		
		char const *begin = nullptr;
		char const *end = nullptr;
		char const *ptr = nullptr;
		std::vector<StateItem> states;
		bool hold = false;
		std::string key;
		std::string string;
		double number = 0;
		bool is_array = false;
		char comment_state = 0; // 0=none, '/'=line comment, '*'=block comment
		bool allow_comment = false;
		bool allow_ambiguous_comma = false;
		bool allow_unquoted_key = false;
		bool allow_hexadecimal = false;
		bool allow_special_constant = false;
		bool allow_key_in_array = false;
		std::vector<std::string> depth;
		struct NestItem {
			int depth;
			std::string path;
		};
		std::vector<NestItem> nest_stack;
		StateItem last_state;
		std::vector<Error> errors;
		bool extraction_support = true;
	};
	ParserData d;

	void push_error(std::string const &what)
	{
		d.states.clear();

		Error err;
		err.what_ = what;
		if (d.ptr && d.begin && d.ptr >= d.begin) {
			err.offset = static_cast<size_t>(d.ptr - d.begin);
			size_t line = 1;
			size_t column = 1;
			for (char const *p = d.begin; p < d.ptr; ++p) {
				if (*p == '\n') {
					line++;
					column = 1;
				} else if (*p != '\r') {
					column++;
				}
			}
			err.line = line;
			err.column = column;
		}
		d.errors.push_back(err);
	}

	void push_state(StateItem s)
	{
		if (state() == Key || state() == Comma || state() == EndObject) {
			d.states.pop_back();
		}
		d.states.push_back(s);

		if (isarray()) {
			d.key.clear();
		}

		switch (s.type) {
		case StartArray:
			d.is_array = true;
			break;
		case StartObject:
		case Key:
			d.is_array = false;
			break;
		}
	}

	bool pop_state()
	{
		bool f = false;
		if (!d.states.empty()) {
			d.last_state = d.states.back();
			d.states.pop_back();
			size_t i = d.states.size();
			while (i > 0) {
				i--;
				auto s = d.states[i];
				if (s.type == StartArray) {
					d.is_array = true;
					break;
				}
				if (s.type == StartObject || s.type == Key) {
					d.is_array = false;
					break;
				}
			}
			f = true;
			if (state() == Key) {
				d.states.pop_back();
			}
		}
		d.key.clear();
		return f;
	}

	void parse(char const *begin, char const *end)
	{
		reset();
		d = {};
		d.begin = begin;
		d.ptr = begin;
		d.end = end;
	}

	void parse(std::string_view sv)
	{
		parse(sv.data(), sv.data() + sv.size());
	}

	void parse(char const *ptr, int len = -1)
	{
		if (len < 0) {
			len = (int)strlen(ptr);
		}
		parse(ptr, ptr + len);
	}
	
	bool _internal_next()
	{
		bool not_enough_input = true;
		while (d.ptr < d.end) {
			not_enough_input = false;
			
			if (skip_space()) continue;
			
			if (*d.ptr == '}') {
				d.ptr++;
				d.string.clear();
				std::string key;
				if (!d.depth.empty()) {
					key = d.depth.back();
					auto n = key.size();
					if (n > 0) {
						if (key[n - 1] == '{') {
							key = key.substr(0, n - 1);
						}
					}
					d.depth.pop_back();
				}
				while (1) {
					bool f = (state() == StartObject);
					if (!pop_state()) break;
					if (f) {
						push_state(EndObject);
						d.key = key;
						return true;
					}
				}
			}
			if (*d.ptr == ']') {
				d.ptr++;
				d.string.clear();
				std::string key;
				if (!d.depth.empty()) {
					key = d.depth.back();
					auto n = key.size();
					if (n > 0) {
						if (key[n - 1] == '[') {
							key = key.substr(0, n - 1);
						}
					}
					d.depth.pop_back();
				}
				while (1) {
					bool f = (state() == StartArray);
					if (!pop_state()) break;
					if (f) {
						push_state(EndArray);
						d.key = key;
						return true;
					}
				}
			}
			if (*d.ptr == ',') {
				d.ptr++;
				if (state() == Key) {
					push_state(Null);
					return true;
				}
				skip_space();
				if (is_value()) {
					pop_state();
				}
				push_state(Comma);
				if (d.allow_ambiguous_comma) {
					continue;
				} else {
					// if not allow_ambiguous_comma, fall through
				}
			}
			if (*d.ptr == '{') {
				char const *p = d.ptr++;
				if (state() != Key) {
					d.key.clear();
					d.string.clear();
				}
				d.depth.push_back(d.key + '{');
				push_state({StartObject, p});
				return true;
			}
			if (*d.ptr == '[') {
				char const *p = d.ptr++;
				if (state() != Key) {
					d.key.clear();
					d.string.clear();
				}
				d.depth.push_back(d.key + '[');
				push_state({StartArray, p});
				return true;
			}
			if (*d.ptr == '\"') {
				switch (state()) {
				case None:
				case StartObject:
				case StartArray:
				case Key:
				case Comma:
					// nop
					break;
				default:
					if (d.allow_ambiguous_comma) {
						break; // consider as a virtual comma is exists
					}
					push_error("unexpected double quote");
					return false;
				}
				
				auto n = parse_string(d.ptr, d.end, &d.string);
				if (n == 0 || d.ptr + n == d.end) {
					not_enough_input = true;
					break;
				}
				if (n > 0) {
					d.ptr += n;
					skip_space();
					if (state() == Key) {
						//
					} else {
						int c = peek_next_char();
						if (c < 0) {
							not_enough_input = true;
							break;
						}
						if (c == ':') {
							if (isarray()) {
								// unusual syntax; "key":"value" in array
								// e.g. [ "key": "value" ]
								if (!d.allow_key_in_array) {
									push_error("unexpected key in array");
									return false;
								}
							}
							d.ptr++;
							d.key = d.string;
							push_state(Key);
							return true;
						}
					}
					push_state(String);
					return true;
				}
			}
			if (state() == Key || isarray()) {
				auto n = parse_number(d.ptr, d.end, &d.number);
				if (n > 0) {
					if (n == 0 || d.ptr + n == d.end) {
						not_enough_input = true;
						break;
					}
					d.string.assign(d.ptr, n);
					d.ptr += n;
					push_state(Number);
					return true;
				}
				if (std::isalpha(static_cast<unsigned char>(*d.ptr))) {
					auto n = parse_symbol(d.ptr, d.end, &d.string);
					if (n == 0 || d.ptr + n == d.end) {
						not_enough_input = true;
						break;
					}
					if (n > 0) {
						if (state() == Key || state() == Comma || state() == StartArray) {
							d.ptr += n;
							if (d.string == "false") {
								push_state(False);
								return true;
							}
							if (d.string == "true") {
								push_state(True);
								return true;
							}
							if (d.string == "null") {
								push_state(Null);
								return true;
							}
						}
					}
				}
		} else if (d.allow_unquoted_key) {
			auto n = parse_symbol(d.ptr, d.end, &d.string);
			if (n == 0 || d.ptr + n == d.end) {
				not_enough_input = true;
				break;
			}
			if (n > 0) {
				d.ptr += n;
				skip_space();
				if (d.ptr < d.end && *d.ptr == ':') {
					d.ptr++;
					d.key = d.string;
					push_state(Key);
					return true;
				}
			}
		}
			if (!has_error()) {
				push_error("syntax error");
			}
			d.not_enough_input = true;
			return false;
		}
		
		if ((state() == EndObject || state() == EndArray) && d.depth.empty()) {
			push_state(EndDocument);
			return false;
		}
		
		if (not_enough_input) {
			d.not_enough_input = true;
			need_input();
			return false;
		}
		
		return false;
	}
	static void _init(ParserData *d)
	{
		d->begin = nullptr;
		d->end = nullptr;
		d->ptr = nullptr;
	}
public:
	Reader() = default;
	Reader(std::string_view sv)
	{
		parse(sv);
	}
	Reader(char const *begin, char const *end)
	{
		parse(begin, end);
	}
	Reader(char const *ptr, int len = -1)
	{
		parse(ptr, len);
	}
	Reader(Reader &&r)
		: d(std::move(r.d))
	{
		_init(&r.d);
	}
	Reader &operator=(Reader &&r)
	{
		if (this != &r) {
			d = std::move(r.d);
			_init(&r.d);
		}
		return *this;
	}
	Reader(Reader const &r) = delete;
	Reader &operator=(Reader const &r) = delete;
	
	Reader(std::function<void ()> fn_input_calback)
	{
		d.fn_input_calback = fn_input_calback;
		d.extraction_support = false;
	}
	
	void input(std::string_view in)
	{
		if (in.empty()) return;
		
		static constexpr size_t EXTRA_ROOM = 200; // reserve extra room to avoid frequent reallocations
		
		d.not_enough_input = false;
		d.extraction_support = false;
		
		if (d.input_buffer && (d.input_buffer->capacity() - d.input_buffer->size()) >= in.size()) {
			if (d.ptr && d.end && d.ptr == d.end) {
				// all previous input has been consumed, reuse the buffer
				d.input_buffer->assign(in.begin(), in.end());
				d.begin = d.ptr = d.input_buffer->data();
			} else {
				// append new input to the existing buffer
				d.input_buffer->insert(d.input_buffer->end(), in.begin(), in.end());
				if (!d.begin) d.begin = d.input_buffer->data();
				if (!d.ptr)   d.ptr = d.input_buffer->data();
			}
			d.end = d.input_buffer->data() + d.input_buffer->size();
		} else {
			std::vector<char> newbuf;
			size_t curr = (d.ptr && d.end) ? (d.end - d.ptr) : 0;
			newbuf.reserve(curr + in.size() + EXTRA_ROOM);
			if (curr > 0) {
				newbuf.assign(d.ptr, d.end); // copy remaining unprocessed data to the new buffer
			}
			newbuf.insert(newbuf.end(), in.begin(), in.end()); // append new input data
			d.input_buffer = std::move(newbuf);
			d.begin = d.input_buffer->data();
			d.ptr = d.begin;
			d.end = d.begin + d.input_buffer->size();
		}
	}
	
	void allow_comment(bool allow)
	{
		d.allow_comment = allow;
	}
	void allow_ambiguous_comma(bool allow)
	{
		d.allow_ambiguous_comma = allow;
	}
	void allow_unquoted_key(bool allow)
	{
		d.allow_unquoted_key = allow;
	}
	void allow_hexadecimal(bool allow)
	{
		d.allow_hexadecimal = allow;
	}
	void allow_special_constant(bool allow)
	{
		d.allow_special_constant = allow;
	}
	void allow_key_in_array(bool allow)
	{
		d.allow_key_in_array = allow;
	}
	void reset()
	{
		d.errors.clear();
	}
	void hold()
	{
		d.hold = true;
	}
	void nest()
	{
		ParserData::NestItem item;
		item.depth = depth();
		item.path = path();
		d.nest_stack.push_back(item);
	}
	void nest(std::function<void ()> callback_fn)
	{
		nest();
		do {
			callback_fn();
		} while (next());
	}
	bool next()
	{
		if (d.hold) {
			d.hold = false;
			return true;
		}
		if (_internal_next()) {
			if (d.nest_stack.empty()) return true;
			if (this->depth() >= d.nest_stack.back().depth) {
				return true;
			}
			d.nest_stack.pop_back();
			hold();
		}
		if (is_streaming_input_mode() && !is_not_enough_input()) {
			if (state() != EndDocument) {
				return true;
			}
		}
		return false;
	}
	void next_document()
	{
		if (state() == EndDocument) {
			d.states.clear();
		}
	}

	StateType state() const
	{
		return d.states.empty() ? None : d.states.back().type;
	}

	bool has_error() const
	{
		return !d.errors.empty();
	}

	std::vector<Error> const &errors() const
	{
		return d.errors;
	}
	
	bool is_not_enough_input() const
	{
		return d.not_enough_input;
	}
	
	bool is_start_object() const
	{
		return state() == StartObject;
	}

	bool is_end_object() const
	{
		return state() == EndObject;
	}

	bool is_start_array() const
	{
		return state() == StartArray;
	}

	bool is_end_array() const
	{
		return state() == EndArray;
	}

	bool is_constant() const
	{
		switch (state()) {
		case String:
		case Number:
		case Null:
		case False:
		case True:
			return true;
		}
		return false;
	}

	bool is_structure() const
	{
		switch (state()) {
		case StartObject:
		case StartArray:
			return true;
		case EndObject:
		case EndArray:
			if (d.states.size() > 1) {
				auto s = d.states[d.states.size() - 2];
				switch (s.type) {
				case StartObject:
				case StartArray:
					return true;
				}
			}
			break;
		}
		return false;
	}

	bool is_value() const
	{
		return is_constant() || is_structure();
	}

	std::string key() const
	{
		return d.key;
	}

	std::string string() const
	{
		return d.string;
	}

	StateType symbol() const
	{
		StateType s = state();

		switch (s) {
		case Null:
		case False:
		case True:
			return s;
		}
		return None;
	}

	bool isnull() const
	{
		return symbol() == Null;
	}
	
	bool isnumber() const
	{
		return state() == Number;
	}
	
	bool isstring() const
	{
		return state() == String;
	}
	
	bool isfalse() const
	{
		return symbol() == False;
	}

	bool istrue() const
	{
		return symbol() == True;
	}

	bool isboolean() const
	{
		return istrue() || isfalse();
	}
	
	double number() const
	{
		return d.number;
	}

	bool boolean() const
	{
		return istrue();
	}
	
	bool isarray() const
	{
		return d.is_array;
	}

	int depth() const
	{
		return (int)d.depth.size();
	}

	std::string path() const
	{
		std::string path;
		for (std::string const &s : d.depth) {
			path += s;
		}
		if (state() == jstream::StartObject || state() == jstream::StartArray) {
			return path;
		}
		return path + d.key;
	}

	std::string_view extract()
	{
		if (d.last_state.ptr) {
			size_t n = d.ptr - d.last_state.ptr;
			return std::string_view(d.last_state.ptr, n);
		}
		return {};
	}

	bool match(std::string_view path, bool match_end_structure = false) const
	{
		if (!is_value()) return false;
		
		std::string at;
		if (!path.empty() && path.front() == '@') {
			if (!d.nest_stack.empty()) {
				at = d.nest_stack.back().path + std::string(path.substr(1));
				path = at;
			}
		}
		
		auto Path = [&](size_t i){ return i < path.size() ? path[i] : 0; };

		const auto stat = state();

		size_t i;
		for (i = 0; i < d.depth.size(); i++) {
			std::string const &element = d.depth[i];
			if (element.empty()) return false; // something wrong
			if (Path(0) == '*') {
				if (Path(1) == '*') {
					if (Path(2) == 0) return true; // "**" matches any path
					return false; // path syntax error: "**" must be at the end of path
				}
				char c = element.c_str()[element.size() - 1]; // last character of element
				if (c == '{' || c == '[') { // object or array
					if (Path(1) == c) {
						path = path.substr(2); // remove "*{" or "*["
						continue;
					}
					if (Path(1) == 0) {
						if (i + 1 == d.depth.size()) {
							if (c == '{' && stat == StartObject) return true;
							if (c == '[' && stat == StartArray) return true;
						}
						return false; // path syntax error: "*{" or "*[" must be at the end of path if no index specified
					}
				}
			}
			if (path.size() < element.size()) return false;
			if (strncmp(path.data(), element.c_str(), element.size()) != 0) return false;
			path = path.substr(element.size());
		}
		if (Path(0) == '*') {
			if (Path(1) == '*' && Path(2) == 0) return true;
			if (Path(1) == 0 && i == d.depth.size()) {
				if (is_constant()) return true;
				if (match_end_structure) {
					if (stat == EndObject || stat == EndArray) return true;
				}
				return false;
			}
		}
		return path == d.key;
	}

	bool match_start_object(char const *path) const
	{
		return state() == StartObject && match(path);
	}

	bool match_end_object(char const *path) const
	{
		return state() == EndObject && match(path, true);
	}

	bool match_start_array(char const *path) const
	{
		return state() == StartArray && match(path);
	}

	bool match_end_array(char const *path) const
	{
		return state() == EndArray && match(path, true);
	}
	
	uintptr_t tell() const
	{
		return (uintptr_t)d.ptr;
	}
	
	std::string_view extract(uintptr_t begin, uintptr_t end)
	{
		if (!d.extraction_support) {
			push_error("extract() is not supported in streaming input mode");
			return {};
		}
		
		if (begin >= (uintptr_t)d.begin && end <= (uintptr_t)d.end && begin <= end) {
			return std::string_view((char *)begin, end - begin);
		}
		return {};
	}
};

class Writer {
protected:
	void print(char const *p, int n)
	{
		if (output_fn) {
			output_fn(p, n);
		} else {
			string_out.append(p, n);
		}
	}

	void print(char c)
	{
		print(&c, 1);
	}

	void print(char const *p)
	{
		print(p, (int)strlen(p));
	}

	void print(std::string_view s)
	{
		print(s.data(), (int)s.size());
	}
private:
	std::vector<int> stack;
	std::function<void (char const *p, int n)> output_fn;
	std::string string_out;

	bool enable_indent_ = true;
	bool enable_newline_ = true;
	bool allow_nan_ = false;

	void print_newline()
	{
		if (!enable_newline_) return;

		print('\n');
	}

	void print_indent()
	{
		if (!enable_indent_) return;

		size_t n = stack.size() - 1;
		for (size_t i = 0; i < n; i++) {
			print(' ');
			print(' ');
		}
	}

	bool print_number(double v)
	{
		std::string s = format_double(v, allow_nan_);
		if (s.empty()) {
			print("null");
			return false;
		}
		print(s);
		return true;
	}

	void print_string(std::string_view s)
	{
		std::vector<char> buf = encode_json_string(s);

		print('\"');
		if (!buf.empty()) {
			print(buf.data(), (int)buf.size());
		}
		print('\"');
	}
	
	void print_raw(std::string_view s)
	{
		if (!s.empty()) {
			print(s);
		}
	}
	
	bool print_value(std::string const &name, std::function<bool ()> const &fn)
	{
		print_name(name);

		bool ok = fn();

		if (!stack.empty()) {
			stack.back()++;
		}
		if (stack.size() == 1) {
			flush();
		}
		return ok;
	}

	void print_object(std::string const &name = {}, std::function<void ()> const &fn = {})
	{
		print_name(name);
		print('{');
		stack.push_back(0);
		if (fn) {
			fn();
			end_object();
		}
	}

	void print_array(std::string const &name = {}, std::function<void ()> const &fn = {})
	{
		print_name(name);
		print('[');
		stack.push_back(0);
		if (fn) {
			fn();
			end_array();
		}
	}

	void end_block()
	{
		print_newline();
		if (!stack.empty()) {
			stack.pop_back();
			if (!stack.empty()) stack.back()++;
		}
		print_indent();
	}

	void reset()
	{
		stack.clear();
		stack.push_back(0);
	}

	void flush()
	{
		if (!stack.empty() && stack.front() > 0) {
			print_newline();
		}
		reset();
	}
public:
	Writer(std::function<void (char const *p, int n)> fn = {})
	{
		output_fn = fn;
		reset();
	}

	~Writer()
	{
		flush();
	}

	void enable_indent(bool enabled)
	{
		enable_indent_ = enabled;
	}

	void enable_newline(bool enabled)
	{
		enable_newline_ = enabled;
	}

	void allow_nan(bool allow)
	{
		allow_nan_ = allow;
	}

	void print_name(std::string const &name)
	{
		if (!stack.empty()) {
			if (stack.back() > 0) {
				print(',');
			}
		}
		if (stack.size() > 1) {
			print_newline();
		}
		print_indent();
		if (!name.empty()) {
			print_string(name);
			print(':');
			if (enable_indent_) {
				print(' ');
			}
		}
	}

	void start_object(std::string const &name = {})
	{
		print_object(name);
	}

	void end_object()
	{
		end_block();
		print('}');

		if (stack.size() == 1) {
			flush();
		}
	}

	void object(std::string const &name, std::function<void ()> const &fn)
	{
		print_object(name, fn);
	}

	void start_array(std::string const &name = {})
	{
		print_array(name, {});
	}

	void end_array()
	{
		end_block();
		print(']');

		if (stack.size() == 1) {
			flush();
		}
	}

	void array(std::string const &name, std::function<void ()> const &fn)
	{
		print_array(name, fn);
	}

	bool number(std::string const &name, double v)
	{
		return print_value(name, [&](){
			return print_number(v);
		});
	}

	void number(double v)
	{
		number({}, v);
	}

	void string(std::string const &name, std::string_view s)
	{
		print_value(name, [&](){
			print_string(s);
			return true;
		});
	}

	void string(std::string const &s)
	{
		string({}, s);
	}

	void symbol(std::string const &name, StateType v)
	{
		print_value(name, [&](){
			switch (v) {
			case False:
				print("false");
				break;
			case True:
				print("true");
				break;
			default:
				print("null");
			}
			return true;
		});
	}

	void boolean(std::string const &name, bool b)
	{
		symbol(name, b ? True : False);
	}

	void null()
	{
		symbol({}, Null);
	}
	
	void null(std::string const &name)
	{
		symbol(name, Null);
	}

	operator std::string () const
	{
		return string_out;
	}
	
	void raw(std::string const &name, std::string_view s)
	{
		print_value(name, [&](){
			print_raw(s);
			return true;
		});
	}
};

typedef std::nullptr_t null_t;
static constexpr std::nullptr_t null = nullptr;

struct Array;
struct KeyValue;
typedef std::vector<KeyValue> _Object;
typedef std::variant<null_t, bool, double, std::string, _Object, Array> Variant;

inline bool operator == (jstream::Variant const &lhs, jstream::Variant const &rhs);

struct Array {
	std::vector<Variant> a;
	size_t size() const
	{
		return a.size();
	}
	bool empty() const
	{
		return a.empty();
	}
	Variant &operator[](size_t i)
	{
		return a[i];
	}
	Variant const &operator[](size_t i) const
	{
		return a[i];
	}
	template <typename T> T &get(size_t i)
	{
		assert(i < a.size());
		return std::get<T>(a[i]);
	}
	template <typename T> T const &get(size_t i) const
	{
		assert(i < a.size());
		return std::get<T>(a[i]);
	}
	void push_back(Variant const &v);
	Array &operator += (Variant const &v)
	{
		push_back(v);
		return *this;
	}
};
struct KeyValue {
	std::string key;
	Variant value;
	KeyValue() = default;
	KeyValue(std::string const &k, Variant const &v)
		: key(k), value(v)
	{
	}
	bool operator == (jstream::KeyValue const &rhs) const;
};
inline void Array::push_back(const Variant &v)
{
	a.push_back(v);
}
struct VariantRef {
	Variant *var;
	VariantRef(Variant &v)
		: var(&v)
	{
	}
	void operator = (Variant const &v)
	{
		*var = v;
	}
	operator Variant &()
	{
		return *var;
	}
};

struct Object {
	_Object *p;
	Object() : p(nullptr)
	{
	}
	Object(_Object &o)
		: p(&o)
	{
	}
	Object(Variant &v)
	{
		if (!std::holds_alternative<_Object>(v)) {
			v = _Object();
		}
		p = &std::get<_Object>(v);
	}
	size_t size() const
	{
		return p ? p->size() : 0;
	}
	bool empty() const
	{
		return size() == 0;
	}
	Variant *find(std::string const &key)
	{
		if (p) {
			for (auto &kv : *p) {
				if (kv.key == key) {
					return &kv.value;
				}
			}
		}
		return nullptr;
	}
	Variant const *find(std::string const &key) const
	{
		return const_cast<Object *>(this)->find(key);
	}
	Variant &value(std::string const &key)
	{
		Variant *v = find(key);
		assert(v);
		return *v;
	}
	Variant const &value(std::string const &key) const
	{
		return const_cast<Object *>(this)->value(key);
	}
	template <typename T> T const &get(std::string const &key) const
	{
		Variant const *v = find(key);
		assert(v);
		return std::get<T>(*v);
	}
	VariantRef operator [] (std::string const &key)
	{
		if (Variant *v = find(key)) {
			return VariantRef(*v);
		}
		p->emplace_back(key, Variant());
		return p->back().value;
	}
};

static inline bool is_null(Variant const &v)
{
	return std::holds_alternative<null_t>(v);
}
static inline bool is_boolean(Variant const &v)
{
	return std::holds_alternative<bool>(v);
}
static inline bool is_number(Variant const &v)
{
	return std::holds_alternative<double>(v);
}
static inline bool is_string(Variant const &v)
{
	return std::holds_alternative<std::string>(v);
}
static inline bool is_object(Variant const &v)
{
	return std::holds_alternative<_Object>(v);
}
static inline bool is_array(Variant const &v)
{
	return std::holds_alternative<Array>(v);
}
static inline bool is_nan(Variant const &v)
{
	return std::holds_alternative<double>(v) && std::isnan(std::get<double>(v));
}
static inline bool is_infinite(Variant const &v)
{
	return std::holds_alternative<double>(v) && std::isinf(std::get<double>(v));
}
static inline Array &arr(Array &a)
{
	return a;
}
static inline Array &arr(Variant &v)
{
	if (!std::holds_alternative<Array>(v)) {
		v = Array();
	}
	return std::get<Array>(v);
}
static inline Object obj(Variant &v)
{
	if (!std::holds_alternative<_Object>(v)) {
		v = _Object();
	}
	return Object(std::get<_Object>(v));
}

static inline Variant var(jstream::Reader const &reader)
{
	if (reader.isnull()) {
		return null;
	} else if (reader.isfalse()) {
		return false;
	} else if (reader.istrue()) {
		return true;
	} else if (reader.isnumber()) {
		return reader.number();
	} else if (reader.isstring()) {
		return reader.string();
	}
	return null;
}

using std::get;

} // namespace jstream

inline bool jstream::KeyValue::operator == (jstream::KeyValue const &rhs) const
{
	return this->key == rhs.key && this->value == rhs.value;
}

inline bool operator == (jstream::KeyValue const &lhs, jstream::KeyValue const &rhs)
{
	return lhs.operator == (rhs);
}

inline bool operator == (jstream::Variant const &lhs, jstream::Variant const &rhs)
{
	if (lhs.index() != rhs.index()) return false;
	switch (lhs.index()) {
	case 0: return true; // null
	case 1: return std::get<bool>(lhs) == std::get<bool>(rhs);
	case 2: return std::get<double>(lhs) == std::get<double>(rhs);
	case 3: return std::get<std::string>(lhs) == std::get<std::string>(rhs);
	case 4: return std::get<jstream::_Object>(lhs) == std::get<jstream::_Object>(rhs);
	case 5: return std::get<jstream::Array>(lhs).a == std::get<jstream::Array>(rhs).a;
	}
	return false;
}

#endif // JSTREAM_H_
