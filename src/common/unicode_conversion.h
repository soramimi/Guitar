#pragma once

#include <string>
#include <string_view>

std::u16string convert_utf8_to_utf16(std::string_view const &s);
std::string convert_utf16_to_utf8(const std::u16string_view &s);