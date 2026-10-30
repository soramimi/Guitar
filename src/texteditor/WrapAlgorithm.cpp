#include "WrapAlgorithm.h"

#include <assert.h>
#include <cctype>

namespace texteditor::wrap_algorithm {
namespace {

char32_t normalized(char32_t c)
{
	return c == U'\r' ? U'\n' : c;
}

bool is_ascii_space(char32_t c)
{
	return c < 0x80 && std::isspace(static_cast<unsigned char>(c));
}

bool is_ascii_upper(char32_t c)
{
	return c < 0x80 && std::isupper(static_cast<unsigned char>(c));
}

bool is_ascii_alnum(char32_t c)
{
	return c < 0x80 && std::isalnum(static_cast<unsigned char>(c));
}

bool is_breakable(char32_t before, char32_t current)
{
	if (before < 0x80 && current < 0x80) {
		if (is_ascii_space(before) && is_ascii_space(current)) return false;
		if (is_ascii_upper(before) && is_ascii_alnum(current)) return false;
		if (is_ascii_alnum(before)) {
			if (is_ascii_upper(current)) return true;
			if (is_ascii_alnum(current)) return false;
			if (is_ascii_space(current)) return false;
		}
	}
	return true;
}

std::vector<size_t> collect_break_positions(std::u32string_view text)
{
	std::vector<size_t> positions;
	positions.reserve(text.size());
	for (size_t pos = 1; pos < text.size(); pos++) {
		if (is_breakable(normalized(text[pos - 1]), normalized(text[pos]))) {
			positions.push_back(pos);
		}
	}
	// 行末は直前の文字種にかかわらず、常に最後の候補とする。
	positions.push_back(text.size());
	return positions;
}

}

std::vector<size_t> compute_wrap_points(
	std::u32string_view text,
	int width,
	Mode mode,
	Measure const &measure)
{
	std::vector<size_t> result;
	if (text.empty()) return result;

	std::vector<size_t> const break_positions = mode == Mode::WordWrap
		? collect_break_positions(text) : std::vector<size_t>{};
	size_t begin = 0;
	size_t break_index = 0;
	while (begin < text.size()) {
		size_t char_candidate_count = text.size() - begin;
		size_t candidate_count = char_candidate_count;
		if (mode == Mode::WordWrap) {
			// 採用済みの表示行より前にある分割可能位置を読み飛ばす。
			// break_indexは論理行全体を通じて前進するだけなので、全体でO(n)。
			while (break_positions[break_index] <= begin) {
				break_index++;
			}
			assert(break_index < break_positions.size());
			char_candidate_count = break_positions[break_index] - begin;
			// 最初の分割可能位置までは全文字境界、それ以降は分割可能位置だけを使う。
			candidate_count = char_candidate_count
				+ break_positions.size() - break_index - 1;
		}
		assert(candidate_count > 0);

		auto candidate_at = [&](size_t index) {
			assert(index < candidate_count);
			if (mode == Mode::CharWrap || index < char_candidate_count) {
				return begin + index + 1;
			}
			return break_positions[
				break_index + 1 + index - char_candidate_count];
		};

		// 最初の候補すら収まらない場合は、その候補を採用して必ず前進する。
		// どちらのモードも表示行の先頭はCharWrapなので、最初の候補は1コードポイント。
		size_t best = 0;
		if (measure(begin, candidate_at(0)) <= width) {
			// 1, 2, 4, 8...個先を指数探索する。残り全体の中央から測る
			// 二分探索と異なり、各表示行の近傍だけを計測して上限を見つける。
			size_t upper = candidate_count;
			for (size_t step = 1; step < candidate_count;) {
				if (measure(begin, candidate_at(step)) > width) {
					upper = step;
					break;
				}
				best = step;
				if (step > candidate_count / 2) break;
				step *= 2;
			}

			// 最後に収まった候補と、最初に幅を超えた候補（または行末）の
			// 間だけを二分探索する。upper自身は幅超過と判明済みなので含めない。
			size_t lo = best + 1;
			size_t hi = upper;
			while (lo < hi) {
				size_t mid = lo + (hi - lo) / 2;
				if (measure(begin, candidate_at(mid)) <= width) {
					best = mid;
					lo = mid + 1;
				} else {
					hi = mid;
				}
			}
		}

		size_t end = candidate_at(best);
		assert(end > begin);
		assert(end <= text.size());
		result.push_back(end);
		begin = end;
	}

	assert(!result.empty());
	assert(result.back() == text.size());
	return result;
}

}
