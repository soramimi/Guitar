#include "EscapeSequence.h"

#include <limits>
#include <vector>

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

constexpr uint32_t bright_colors[] = {
0x808080, // bright black
0xff0000, // bright red
0x00ff00, // bright green
0xffff00, // bright yellow
0x0000ff, // bright blue
0xff00ff, // bright magenta
0x00ffff, // bright cyan
0xffffff, // bright white
};

uint32_t indexed_color(int index)
{
	if (index < 0 || index > 255) return 0;
	if (index < 8) return colors[index];
	if (index < 16) return bright_colors[index - 8];
	if (index < 232) {
		int const cube = index - 16;
		auto component = [](int value){
			return value == 0 ? 0 : 55 + value * 40;
		};
		int const red = component(cube / 36);
		int const green = component((cube / 6) % 6);
		int const blue = component(cube % 6);
		return static_cast<uint32_t>((red << 16) | (green << 8) | blue);
	}
	int const gray = 8 + (index - 232) * 10;
	return static_cast<uint32_t>((gray << 16) | (gray << 8) | gray);
}
}

int EscapeSequence::parse_int(std::string_view sv) const
{
	int ret = 0;
	for (char c : sv) {
		if (c < '0' || c > '9') return -1;
		int const digit = c - '0';
		if (ret > (std::numeric_limits<int>::max() - digit) / 10) return -1;
		ret = ret * 10 + digit;
	}
	return ret;
}

void EscapeSequence::update_sgr(std::string_view sv)
{
	std::vector<int> params;
	size_t start = 0;
	while (true) {
		size_t end = sv.find(';', start);
		if (end == std::string_view::npos) end = sv.size();
		params.push_back(parse_int(sv.substr(start, end - start)));
		if (end == sv.size()) break;
		start = end + 1;
	}

	auto SetColor = [&](bool foreground, uint32_t value){
		if (foreground) {
			color_ = value;
			has_color_ = true;
		} else {
			background_color_ = value;
			has_background_color_ = true;
		}
	};
	auto ClearColor = [&](bool foreground){
		if (foreground) {
			color_ = 0;
			has_color_ = false;
		} else {
			background_color_ = 0;
			has_background_color_ = false;
		}
	};

	for (size_t i = 0; i < params.size(); i++) {
		int const num = params[i];
		if (num == 0) {
			ClearColor(true);
			ClearColor(false);
		} else if (num >= 30 && num <= 37) {
			SetColor(true, colors[num - 30]);
		} else if (num == 39) {
			ClearColor(true);
		} else if (num >= 40 && num <= 47) {
			SetColor(false, colors[num - 40]);
		} else if (num == 49) {
			ClearColor(false);
		} else if (num >= 90 && num <= 97) {
			SetColor(true, bright_colors[num - 90]);
		} else if (num >= 100 && num <= 107) {
			SetColor(false, bright_colors[num - 100]);
		} else if (num == 38 || num == 48) {
			bool const foreground = num == 38;
			if (i + 1 >= params.size()) continue;
			int const selector = params[++i];
			if (selector == 5) {
				if (i + 1 < params.size()) {
					int const index = params[++i];
					if (index >= 0 && index <= 255) {
						SetColor(foreground, indexed_color(index));
					}
				}
			} else if (selector == 2) {
				if (i + 3 < params.size()) {
					int const red = params[i + 1];
					int const green = params[i + 2];
					int const blue = params[i + 3];
					i += 3;
					if (red >= 0 && red <= 255 && green >= 0 && green <= 255
						&& blue >= 0 && blue <= 255) {
						SetColor(foreground, static_cast<uint32_t>(
							(red << 16) | (green << 8) | blue));
					}
				} else {
					// 不完全なtruecolor指定の残りを独立したSGRとして解釈しない。
					i = params.size() - 1;
				}
			}
		}
	}
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
		if (chars_.size() == 1 && c != '[') {
			// CSI以外のESCシーケンスは扱わない。ESCだけを捨て、現在の文字は
			// 通常文字として呼び出し元に処理させる。
			clear();
			return false;
		}

		bool const is_csi = chars_.size() >= 2
			&& chars_[0] == '\x1b' && chars_[1] == '[';
		unsigned char const byte = static_cast<unsigned char>(c);
		if (is_csi && byte >= 0x40 && byte <= 0x7e) {
			std::string_view sv(chars_.data() + 2, chars_.size() - 2);
			if (c == 'm') {
				// SGRは色に関係しない属性も含め、シーケンス全体を消費する。
				// 引数なしと空引数はSGR 0として扱う。
				update_sgr(sv);
			}
			// カーソル表示制御など未実装のCSIも本文へ終端文字を漏らさない。
			clear();
			return true;
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

uint32_t EscapeSequence::background_color() const
{
	return background_color_;
}

bool EscapeSequence::has_background_color() const
{
	return has_background_color_;
}
