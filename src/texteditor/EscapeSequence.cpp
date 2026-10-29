#include "EscapeSequence.h"

namespace {
constexpr uint32_t colors[] = {
0x000000, // black
0x800000, // red
0x008000, // green
0x808000, // yellow
0x000080, // blue
0x800080, // magenta
0x008080, // cyan
0xc0c0c0, // white
};
}

int EscapeSequence::parse_int(std::string_view sv) const
{
	int ret = 0;
	for (char c : sv) {
		if (c < '0' || c > '9') return -1;
		ret = ret * 10 + (c - '0');
	}
	return ret;
}

EscapeSequence::EscapeSequence()
{
	chars_.reserve(20);
}

void EscapeSequence::clear()
{
	chars_.clear();
}

bool EscapeSequence::update(char c)
{
	if (chars_.empty()) {
		if (c == '\x1b') {
			chars_.push_back(c);
		}
	} else {
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
			bool consumed = false;
			if (chars_.size() >= 2 && chars_[0] == 0x1b && chars_[1] == '[') {
				std::string_view sv(chars_.data() + 2, chars_.size() - 2);
				if (c == 'm') {
					// SGRは色に関係しない属性も含め、終端の'm'を本文へ
					// 流出させない。引数なしと空引数はSGR 0として扱う。
					consumed = true;
					size_t start = 0;
					while (true) {
						size_t end = sv.find(';', start);
						if (end == std::string_view::npos) end = sv.size();
						int num = parse_int(sv.substr(start, end - start));
						if (num == 0) {
							color_ = 0;
							has_color_ = false;
						} else if (num >= 30 && num <= 37) {
							color_ = colors[num - 30];
							has_color_ = true;
						}
						if (end == sv.size()) break;
						start = end + 1;
					}
				}
			}
			clear();
			return consumed;
		} else {
			chars_.push_back(c);
		}
	}
	return false;
}

bool EscapeSequence::empty() const
{
	return chars_.empty();
}

uint32_t EscapeSequence::color() const
{
	return color_;
}

bool EscapeSequence::has_color() const
{
	return has_color_;
}
