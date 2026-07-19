#include "wstring.h"

#ifdef _WIN32
#include <Windows.h>

std::wstring misc::convert_str_to_wstr(std::string_view const &str)
{
	std::wstring wstr;
	if (str.empty()) return wstr;
	int len = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), nullptr, 0);
	if (len > 0) {
		wstr.resize(len);
		MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), &wstr[0], len);
	}
	return wstr;
}

std::string misc::convert_wstr_to_str(std::wstring_view const &str)
{
	std::string s;
	if (str.empty()) return s;
	int len = WideCharToMultiByte(CP_UTF8, 0, str.data(), (int)str.size(), nullptr, 0, nullptr, nullptr);
	if (len > 0) {
		s.resize(len);
		WideCharToMultiByte(CP_UTF8, 0, str.data(), (int)str.size(), &s[0], len, nullptr, nullptr);
	}
	return s;
}

#else

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <stdexcept>
#include <string>
#include <string_view>


std::u16string misc::convert_str_to_wstr(const std::string_view &str)
{
	std::u16string result;
	result.reserve(str.size());

	const auto *p = reinterpret_cast<const unsigned char *>(str.data());
	const auto *end = p + str.size();

	while (p < end) {
		char32_t codepoint;
		std::size_t length;

		if (*p <= 0x7f) {
			codepoint = *p;
			length = 1;
		} else if ((*p & 0xe0) == 0xc0) {
			codepoint = *p & 0x1f;
			length = 2;
		} else if ((*p & 0xf0) == 0xe0) {
			codepoint = *p & 0x0f;
			length = 3;
		} else if ((*p & 0xf8) == 0xf0) {
			codepoint = *p & 0x07;
			length = 4;
		} else {
			throw std::invalid_argument("Invalid UTF-8 leading byte");
		}

		if (static_cast<std::size_t>(end - p) < length) {
			throw std::invalid_argument("Truncated UTF-8 sequence");
		}

		for (std::size_t i = 1; i < length; ++i) {
			if ((p[i] & 0xc0) != 0x80) {
				throw std::invalid_argument("Invalid UTF-8 continuation byte");
			}

			codepoint = (codepoint << 6) | (p[i] & 0x3f);
		}

		// 過長形式を拒否する。
		static constexpr char32_t minimum_codepoint[] = {
			0x000000,
			0x000000,
			0x000080,
			0x000800,
			0x010000,
		};

		if (codepoint < minimum_codepoint[length]) {
			throw std::invalid_argument("Overlong UTF-8 sequence");
		}

		// Unicodeスカラー値として不正な値を拒否する。
		if (codepoint > 0x10ffff ||
			(codepoint >= 0xd800 && codepoint <= 0xdfff)) {
			throw std::invalid_argument("Invalid Unicode code point");
		}

		if (codepoint <= 0xffff) {
			result.push_back(static_cast<char16_t>(codepoint));
		} else {
			codepoint -= 0x10000;

			result.push_back(static_cast<char16_t>(
				0xd800 + (codepoint >> 10)));

			result.push_back(static_cast<char16_t>(
				0xdc00 + (codepoint & 0x3ff)));
		}

		p += length;
	}

	return result;
}

std::string misc::convert_wstr_to_str(std::u16string_view const &str)
{
	std::string result;
	result.reserve(str.size() * 3);

	for (std::size_t i = 0; i < str.size(); ++i) {
		char32_t codepoint = str[i];

		if (codepoint >= 0xd800 && codepoint <= 0xdbff) {
			// 上位サロゲート。後続に下位サロゲートが必要。
			if (i + 1 >= str.size()) {
				throw std::invalid_argument(
					"Unpaired UTF-16 high surrogate");
			}

			const char32_t low = str[i + 1];

			if (low < 0xdc00 || low > 0xdfff) {
				throw std::invalid_argument(
					"Invalid UTF-16 surrogate pair");
			}

			codepoint =
				0x10000 +
				((codepoint - 0xd800) << 10) +
				(low - 0xdc00);

			++i;
		} else if (codepoint >= 0xdc00 && codepoint <= 0xdfff) {
			// 対応する上位サロゲートがない。
			throw std::invalid_argument(
				"Unpaired UTF-16 low surrogate");
		}

		if (codepoint <= 0x7f) {
			result.push_back(static_cast<char>(codepoint));
		} else if (codepoint <= 0x7ff) {
			result.push_back(static_cast<char>(
				0xc0 | (codepoint >> 6)));
			result.push_back(static_cast<char>(
				0x80 | (codepoint & 0x3f)));
		} else if (codepoint <= 0xffff) {
			result.push_back(static_cast<char>(
				0xe0 | (codepoint >> 12)));
			result.push_back(static_cast<char>(
				0x80 | ((codepoint >> 6) & 0x3f)));
			result.push_back(static_cast<char>(
				0x80 | (codepoint & 0x3f)));
		} else {
			result.push_back(static_cast<char>(
				0xf0 | (codepoint >> 18)));
			result.push_back(static_cast<char>(
				0x80 | ((codepoint >> 12) & 0x3f)));
			result.push_back(static_cast<char>(
				0x80 | ((codepoint >> 6) & 0x3f)));
			result.push_back(static_cast<char>(
				0x80 | (codepoint & 0x3f)));
		}
	}

	return result;
}

#endif

