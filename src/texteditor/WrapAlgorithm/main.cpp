#include "../WrapAlgorithm.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cctype>
#include <random>
#include <string>

using texteditor::wrap_algorithm::Mode;

namespace {

char32_t normalized(char32_t c)
{
	return c == U'\r' ? U'\n' : c;
}

bool ascii_space(char32_t c)
{
	return c < 0x80 && std::isspace(static_cast<unsigned char>(c));
}

bool ascii_upper(char32_t c)
{
	return c < 0x80 && std::isupper(static_cast<unsigned char>(c));
}

bool ascii_alnum(char32_t c)
{
	return c < 0x80 && std::isalnum(static_cast<unsigned char>(c));
}

bool breakable(char32_t before, char32_t current)
{
	if (before < 0x80 && current < 0x80) {
		if (ascii_space(before) && ascii_space(current)) return false;
		if (ascii_upper(before) && ascii_alnum(current)) return false;
		if (ascii_alnum(before)) {
			if (ascii_upper(current)) return true;
			if (ascii_alnum(current)) return false;
			if (ascii_space(current)) return false;
		}
	}
	return true;
}

std::vector<size_t> reference_candidates(std::u32string_view text, size_t begin, Mode requested_mode)
{
	std::vector<size_t> candidates;
	Mode current_mode = Mode::CharWrap;
	size_t pos = begin;
	while (pos < text.size()) {
		char32_t current = normalized(text[pos]);
		size_t next = pos + 1;
		if (current_mode == Mode::CharWrap && pos > begin
			&& breakable(normalized(text[pos - 1]), current)) {
			current_mode = requested_mode;
		}
		if (current_mode == Mode::WordWrap) {
			char32_t previous = current;
			while (next <= text.size()) {
				char32_t following = next < text.size() ? normalized(text[next]) : U'\x80';
				if (breakable(previous, following)) break;
				previous = following;
				next++;
			}
		}
		candidates.push_back(next);
		pos = next;
	}
	return candidates;
}

template <typename Measure>
std::vector<size_t> reference_wrap(std::u32string_view text, int width, Mode mode, Measure measure)
{
	std::vector<size_t> result;
	size_t begin = 0;
	while (begin < text.size()) {
		std::vector<size_t> candidates = reference_candidates(text, begin, mode);
		size_t best = 0;
		bool found_fitting = false;
		for (size_t i = 0; i < candidates.size(); i++) {
			if (measure(begin, candidates[i]) > width) break;
			best = i;
			found_fitting = true;
		}
		if (!found_fitting) best = 0;
		begin = candidates[best];
		result.push_back(begin);
	}
	return result;
}

std::vector<std::u32string> split(std::u32string_view text, std::vector<size_t> const &points)
{
	std::vector<std::u32string> result;
	size_t begin = 0;
	for (size_t end : points) {
		result.emplace_back(text.substr(begin, end - begin));
		begin = end;
	}
	return result;
}

}

TEST(WrapAlgorithm, EmptyTextHasNoPoints)
{
	auto measure = [](size_t from, size_t to){ return static_cast<int>(to - from); };
	EXPECT_TRUE(texteditor::wrap_algorithm::compute_wrap_points(U"", 5, Mode::CharWrap, measure).empty());
}

TEST(WrapAlgorithm, CharWrapUsesEveryCharacterBoundary)
{
	auto measure = [](size_t from, size_t to){ return static_cast<int>(to - from); };
	EXPECT_EQ(
		texteditor::wrap_algorithm::compute_wrap_points(U"abcdef", 2, Mode::CharWrap, measure),
		(std::vector<size_t>{2, 4, 6}));
	EXPECT_EQ(
		texteditor::wrap_algorithm::compute_wrap_points(U"abc", 0, Mode::CharWrap, measure),
		(std::vector<size_t>{1, 2, 3}));
}

TEST(WrapAlgorithm, WordWrapRestartsAsCharWrapOnEveryVisualLine)
{
	std::u32string const text = U"ab cdefghij kl mn op";
	auto measure = [](size_t from, size_t to){ return static_cast<int>(to - from); };
	auto points = texteditor::wrap_algorithm::compute_wrap_points(text, 5, Mode::WordWrap, measure);
	EXPECT_EQ(split(text, points),
		(std::vector<std::u32string>{U"ab ", U"cdefg", U"hij ", U"kl ", U"mn op"}));
}

TEST(WrapAlgorithm, WordWithoutBreakablePositionRemainsCharWrap)
{
	std::u32string const text = U"abcdefghij";
	auto measure = [](size_t from, size_t to){ return static_cast<int>(to - from); };
	auto points = texteditor::wrap_algorithm::compute_wrap_points(text, 4, Mode::WordWrap, measure);
	EXPECT_EQ(split(text, points), (std::vector<std::u32string>{U"abcd", U"efgh", U"ij"}));
}

TEST(WrapAlgorithm, LongWordAtVisualLineStartUsesCharWrap)
{
	std::u32string const text = U"aa bbbbbbbbbbbbbbbb cc";
	auto measure = [](size_t from, size_t to){ return static_cast<int>(to - from); };
	auto points = texteditor::wrap_algorithm::compute_wrap_points(text, 6, Mode::WordWrap, measure);
	EXPECT_EQ(split(text, points),
		(std::vector<std::u32string>{U"aa ", U"bbbbbb", U"bbbbbb", U"bbbb ", U"cc"}));
}

TEST(WrapAlgorithm, MatchesGreedyReferenceForRandomInputs)
{
	std::mt19937 random(0x574150u);
	std::u32string const alphabet = U"abCD09  \t!?\u3042";
	for (int iteration = 0; iteration < 5000; iteration++) {
		std::u32string text;
		int length = static_cast<int>(random() % 80);
		for (int i = 0; i < length; i++) {
			text.push_back(alphabet[random() % alphabet.size()]);
		}
		int width = static_cast<int>(random() % 15) - 2;
		Mode mode = random() % 2 == 0 ? Mode::CharWrap : Mode::WordWrap;
		auto measure = [&](size_t from, size_t to){
			int result = 0;
			for (size_t i = from; i < to; i++) {
				if (text[i] == U'\t') result += 4;
				else if (text[i] >= 0x80 || ascii_upper(text[i])) result += 2;
				else result++;
			}
			return result;
		};
		auto expected = reference_wrap(text, width, mode, measure);
		auto actual = texteditor::wrap_algorithm::compute_wrap_points(text, width, mode, measure);
		ASSERT_EQ(actual, expected) << "iteration=" << iteration;
		if (!actual.empty()) {
			EXPECT_EQ(actual.back(), text.size());
			EXPECT_TRUE(std::is_sorted(actual.begin(), actual.end()));
			EXPECT_EQ(std::adjacent_find(actual.begin(), actual.end()), actual.end());
			size_t begin = 0;
			for (size_t end : actual) {
				if (width > 0 && measure(begin, end) > width) {
					EXPECT_EQ(end - begin, 1u) << "iteration=" << iteration;
				}
				begin = end;
			}
		}
	}
}

TEST(WrapAlgorithm, GallopingSearchKeepsMeasuredTextNearLinearInWordWrap)
{
	std::u32string const text(4000, U'x');
	uint64_t measured_code_points = 0;
	auto measure = [&](size_t from, size_t to){
		measured_code_points += to - from;
		return static_cast<int>(to - from);
	};

	auto points = texteditor::wrap_algorithm::compute_wrap_points(text, 40, Mode::WordWrap, measure);
	ASSERT_EQ(points.back(), text.size());
	EXPECT_LT(measured_code_points, text.size() * 12);
}
