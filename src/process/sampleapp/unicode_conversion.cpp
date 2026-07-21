#include "unicode_conversion.h"

namespace {

constexpr char32_t REPLACEMENT_CHARACTER = 0xfffd;
constexpr char32_t MAX_UNICODE_SCALAR_VALUE = 0x10ffff;

/**
 * @brief Checks whether a byte is a valid UTF-8 continuation byte.
 * @param byte Byte value to inspect.
 * @return true if the byte matches the 10xxxxxx continuation pattern.
 */
inline bool is_continuation_byte(unsigned char byte)
{
	return (byte & 0xc0) == 0x80;
}

/**
 * @brief Verifies that a code point is a Unicode scalar value.
 * @param code_point Code point to validate.
 * @return true if the code point is within range and not a surrogate.
 */
inline bool is_valid_unicode_scalar(char32_t code_point)
{
	return code_point <= MAX_UNICODE_SCALAR_VALUE && !(code_point >= 0xd800 && code_point <= 0xdfff);
}

/**
 * @brief Appends a Unicode scalar value to a UTF-16 string.
 * @param out Destination UTF-16 string.
 * @param code_point Scalar value to encode.
 */
inline void append_utf16_code_unit(std::u16string *out, char32_t code_point)
{
	// Invalid scalars are normalized to U+FFFD before encoding.
	if (!is_valid_unicode_scalar(code_point)) {
		code_point = REPLACEMENT_CHARACTER;
	}
	if (code_point < 0x10000) {
		out->push_back((char16_t)code_point);
	} else {
		code_point -= 0x10000;
		out->push_back((char16_t)(0xd800 + (code_point >> 10)));
		out->push_back((char16_t)(0xdc00 + (code_point & 0x3ff)));
	}
}

/**
 * @brief Appends a Unicode scalar value to a UTF-8 string.
 * @param out Destination UTF-8 string.
 * @param code_point Scalar value to encode.
 */
void append_utf8_code_unit(std::string *out, char32_t code_point)
{
	// UTF-8 output is limited to Unicode scalar values.
	if (!is_valid_unicode_scalar(code_point)) {
		code_point = REPLACEMENT_CHARACTER;
	}
	if (code_point < 0x80) {
		out->push_back((char)code_point);
	} else if (code_point < 0x800) {
		out->push_back((char)(0xc0 | (code_point >> 6)));
		out->push_back((char)(0x80 | (code_point & 0x3f)));
	} else if (code_point < 0x10000) {
		out->push_back((char)(0xe0 | (code_point >> 12)));
		out->push_back((char)(0x80 | ((code_point >> 6) & 0x3f)));
		out->push_back((char)(0x80 | (code_point & 0x3f)));
	} else {
		out->push_back((char)(0xf0 | (code_point >> 18)));
		out->push_back((char)(0x80 | ((code_point >> 12) & 0x3f)));
		out->push_back((char)(0x80 | ((code_point >> 6) & 0x3f)));
		out->push_back((char)(0x80 | (code_point & 0x3f)));
	}
}

} // namespace

/**
 * @brief Converts a UTF-8 string into UTF-16.
 * @param s Source UTF-8 byte sequence.
 * @return UTF-16 string with malformed input replaced by U+FFFD.
 */
std::u16string convert_utf8_to_utf16(std::string_view const &s)
{
	std::u16string out;
	const char *ptr = s.data();
	const char *end = ptr + s.size();
	while (ptr < end) {
		char32_t c = REPLACEMENT_CHARACTER;
		unsigned char b = (unsigned char)*ptr;
		if (b < 0x80) {
			c = b;
			ptr++;
		} else if ((b & 0xe0) == 0xc0) {
			if (ptr + 1 >= end) {
				ptr = end;
			} else {
				unsigned char b1 = (unsigned char)ptr[1];
				// Reject overlong forms and non-continuation bytes.
				if (is_continuation_byte(b1)) {
					char32_t decoded = ((b & 0x1f) << 6) | (b1 & 0x3f);
					if (decoded >= 0x80) {
						c = decoded;
					}
					ptr += 2;
				} else {
					ptr++;
				}
			}
		} else if ((b & 0xf0) == 0xe0) {
			if (ptr + 2 >= end) {
				ptr = end;
			} else {
				unsigned char b1 = (unsigned char)ptr[1];
				unsigned char b2 = (unsigned char)ptr[2];
				// Reject overlong forms and surrogate code points.
				if (is_continuation_byte(b1) && is_continuation_byte(b2)) {
					char32_t decoded = ((b & 0x0f) << 12) | ((b1 & 0x3f) << 6) | (b2 & 0x3f);
					if (decoded >= 0x800 && !(decoded >= 0xd800 && decoded <= 0xdfff)) {
						c = decoded;
					}
					ptr += 3;
				} else {
					ptr++;
				}
			}
		} else if ((b & 0xf8) == 0xf0) {
			if (ptr + 3 >= end) {
				ptr = end;
			} else {
				unsigned char b1 = (unsigned char)ptr[1];
				unsigned char b2 = (unsigned char)ptr[2];
				unsigned char b3 = (unsigned char)ptr[3];
				// Four-byte sequences must stay within the Unicode range.
				if (is_continuation_byte(b1) && is_continuation_byte(b2) && is_continuation_byte(b3)) {
					char32_t decoded = ((b & 0x07) << 18) | ((b1 & 0x3f) << 12) | ((b2 & 0x3f) << 6) | (b3 & 0x3f);
					if (decoded >= 0x10000 && decoded <= MAX_UNICODE_SCALAR_VALUE) {
						c = decoded;
					}
					ptr += 4;
				} else {
					ptr++;
				}
			}
		} else {
			ptr++;
		}
		append_utf16_code_unit(&out, c);
	}
	return out;
}

/**
 * @brief Converts a UTF-16 string into UTF-8.
 * @param s Source UTF-16 code units.
 * @return UTF-8 string with malformed input replaced by U+FFFD.
 */
std::string convert_utf16_to_utf8(std::u16string_view const &s)
{
	std::string out;
	const char16_t *ptr = s.data();
	const char16_t *end = ptr + s.size();
	while (ptr < end) {
		char32_t c = REPLACEMENT_CHARACTER;
		char16_t b = *ptr;
		if (b < 0xd800 || b >= 0xe000) {
			c = b;
			ptr++;
		} else if (b < 0xdc00) {
			if (ptr + 1 >= end) {
				ptr = end;
			} else {
				char16_t low = ptr[1];
				// Only a well-formed surrogate pair can produce a supplementary scalar.
				if (low >= 0xdc00 && low < 0xe000) {
					c = 0x10000 + (((b - 0xd800) << 10) | (low - 0xdc00));
					ptr += 2;
				} else {
					ptr++;
				}
			}
		} else {
			ptr++;
		}
		append_utf8_code_unit(&out, c);
	}
	return out;
}