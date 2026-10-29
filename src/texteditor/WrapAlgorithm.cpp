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

std::vector<size_t> collect_candidates(std::u32string_view text, Mode requested_mode)
{
	std::vector<size_t> candidates;
	candidates.reserve(text.size());

	Mode current_mode = Mode::CharWrap;
	size_t pos = 0;
	char32_t before = U'\n';
	char32_t current = U'\n';
	while (pos < text.size()) {
		before = current;
		current = normalized(text[pos]);
		size_t next = pos + 1;

		if (current_mode == Mode::CharWrap && pos > 0 && is_breakable(before, current)) {
			current_mode = requested_mode;
		}
		if (current_mode == Mode::WordWrap) {
			char32_t previous = current;
			while (next <= text.size()) {
				// 非ASCIIの番兵は、行末を常に分割可能にする。
				char32_t following = U'\x80';
				if (next < text.size()) {
					following = normalized(text[next]);
				}
				if (is_breakable(previous, following)) break;
				previous = following;
				next++;
			}
		}

		assert(next > pos);
		assert(next <= text.size());
		candidates.push_back(next);
		pos = next;
	}
	return candidates;
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

	std::vector<size_t> const candidates = collect_candidates(text, mode);
	size_t begin = 0;
	size_t candidate_index = 0;
	while (begin < text.size()) {
		assert(candidate_index < candidates.size());
		assert(candidates[candidate_index] > begin);

		// 最初の候補すら収まらない場合は、その候補を採用して必ず前進する。
		// WordWrapでは1単語、CharWrapでは1コードポイントの幅超過となる。
		size_t best = candidate_index;
		if (measure(begin, candidates[candidate_index]) <= width) {
			size_t lo = candidate_index + 1;
			size_t hi = candidates.size();
			while (lo < hi) {
				size_t mid = lo + (hi - lo) / 2;
				if (measure(begin, candidates[mid]) <= width) {
					best = mid;
					lo = mid + 1;
				} else {
					hi = mid;
				}
			}
		}

		size_t end = candidates[best];
		assert(end > begin);
		assert(end <= text.size());
		result.push_back(end);
		begin = end;
		candidate_index = best + 1;
	}

	assert(!result.empty());
	assert(result.back() == text.size());
	return result;
}

}
