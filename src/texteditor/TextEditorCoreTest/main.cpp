#include "../AbstractTextEditorApplication.h"
#include "../EscapeSequence.h"

#include <QApplication>
#include <gtest/gtest.h>

#include <tuple>

namespace {

struct LayoutSnapshot {
	std::vector<std::vector<std::string>> fragments;
	std::vector<uint32_t> wrap_counts;
	std::vector<std::tuple<uint64_t, uint32_t>> logical_to_visual;
	std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> visual_to_logical;
	uint64_t total_logical_rows = 0;
	uint64_t total_visual_rows = 0;
	bool map_valid = false;

	bool operator == (LayoutSnapshot const &) const = default;
};

class LayoutTestApplication : public AbstractTextEditorApplication {
public:
	explicit LayoutTestApplication(size_t thread_count)
	{
		initEditor();
		setLayoutThreadCount(thread_count);

		QFont font("DejaVu Sans");
		font.setPixelSize(14);
		set_fixed_font(font);
		set_text_font(font);

		set_client_size(180, 240, false);
		setContentWidth(90);
		setTextEditorEngine(std::make_shared<TextEditorEngine>());
		setWrappingMode(WrappingMode::WordWrap);
	}

	void load(std::vector<Document::Line> const &lines)
	{
		setDocument(&lines);
	}

	LayoutSnapshot snapshot() const
	{
		LayoutSnapshot result;
		Document const *doc = document();
		LineIndexMap const &map = cx()->line_index_map;
		result.total_logical_rows = map.total_logical_row_count();
		result.total_visual_rows = map.total_visual_row_count();
		result.map_valid = map.validate();

		for (size_t lrow = 0; lrow < doc->logical_lines.size(); lrow++) {
			Document::Line const &line = doc->logical_lines[lrow];
			std::vector<std::string> fragments;
			uint32_t logical_length = 0;
			for (Document::Line const &fragment : line.sp->meta.visual_lines) {
				fragments.emplace_back(fragment.text());
				logical_length += fragment.sp->meta.logical_col_len;
			}
			result.fragments.push_back(std::move(fragments));

			auto value = map.find(static_cast<uint32_t>(lrow));
			result.wrap_counts.push_back(value ? value->value() : 0);
			for (uint32_t lcol = 0; lcol <= logical_length; lcol++) {
				auto pos = map.logical_to_visual(static_cast<uint32_t>(lrow), lcol);
				result.logical_to_visual.emplace_back(pos.vrow, pos.vcol);
			}
		}
		for (uint64_t vrow = 0; vrow < result.total_visual_rows; vrow++) {
			auto pos = map.visual_to_logical(vrow);
			result.visual_to_logical.emplace_back(pos.lrow, pos.lcol, pos.wrap_index);
		}
		return result;
	}

protected:
	texteditor::PositionX currentPixelX() const override
	{
		return {};
	}

	void updateVisibility(update_visibility_option_t const &) override
	{
	}
};

std::vector<Document::Line> layout_test_lines()
{
	std::vector<Document::Line> lines;
	for (int i = 0; i < 256; i++) {
		std::string text;
		switch (i % 5) {
		case 0: text = "alpha beta gamma delta epsilon "; break;
		case 1: text = "initial extraordinarilylongword tail "; break;
		case 2: text = "tab\tseparated\tcolumns "; break;
		case 3: text = "日本語とUnicode😀の折り返し "; break;
		default: text = "CamelCase HTTPServer value "; break;
		}
		text += std::to_string(i);
		if (i % 3 != 0) text.push_back('\n');
		lines.emplace_back(text);
	}
	return lines;
}

}

TEST(TextWidthCache, EvictsLeastRecentlyUsedEntry)
{
	texteditor::Font::TextWidthCache cache;
	for (size_t i = 0; i < cache.max_entries; i++) {
		cache.insert(QString::number(i), static_cast<int>(i));
	}

	int width = -1;
	ASSERT_TRUE(cache.lookup(QString::number(0), &width));
	EXPECT_EQ(width, 0);

	cache.insert("new", 123);
	EXPECT_EQ(cache.size(), cache.max_entries);
	EXPECT_FALSE(cache.lookup(QString::number(1), &width));
	EXPECT_TRUE(cache.lookup(QString::number(0), &width));
	EXPECT_TRUE(cache.lookup("new", &width));
	EXPECT_EQ(width, 123);
}

TEST(TextWidthCache, DoesNotCacheLongKeys)
{
	texteditor::Font::TextWidthCache cache;
	QString text(static_cast<qsizetype>(cache.max_key_length + 1), QChar('x'));
	cache.insert(text, 42);

	int width = -1;
	EXPECT_EQ(cache.size(), 0u);
	EXPECT_FALSE(cache.lookup(text, &width));
}

TEST(Font, TabStopChangeClearsWidthCache)
{
	texteditor::Font font;
	font.text_width_cache_.insert("\t", 80);
	font.set_tab_stop_distance(80);
	EXPECT_EQ(font.text_width_cache_.size(), 1u);

	font.set_tab_stop_distance(40);
	EXPECT_EQ(font.text_width_cache_.size(), 0u);
}

TEST(CharAttr, DistinguishesDefaultAndExplicitBlack)
{
	CharAttr attr;
	EXPECT_FALSE(attr.has_foreground_color());

	attr.set_foreground_color(0x000000);
	EXPECT_TRUE(attr.has_foreground_color());
	EXPECT_EQ(attr.color, 0u);

	attr.clear_foreground_color();
	EXPECT_FALSE(attr.has_foreground_color());
	EXPECT_EQ(attr.color, 0u);
}

TEST(EscapeSequence, ResetClearsExplicitColor)
{
	EscapeSequence sequence;
	for (char c : std::string("\x1b[30m")) sequence.update(c);
	EXPECT_TRUE(sequence.has_color());
	EXPECT_EQ(sequence.color(), 0u);

	for (char c : std::string("\x1b[0m")) sequence.update(c);
	EXPECT_FALSE(sequence.has_color());
}

TEST(Utf16Text, MapsCodePointRangesToUtf16Ranges)
{
	std::u32string const text = U"A😀B";
	texteditor::Utf16Text utf16(text);
	EXPECT_EQ(utf16.mid(0, 3), QString::fromUcs4(text.data(), text.size()));
	EXPECT_EQ(utf16.mid(0, 1), QString("A"));
	EXPECT_EQ(utf16.mid(1, 2), QString::fromUcs4(text.data() + 1, 1));
	EXPECT_EQ(utf16.mid(2, 3), QString("B"));
}

TEST(Layout, SingleAndEightThreadsProduceIdenticalResults)
{
	std::vector<Document::Line> const lines = layout_test_lines();
	LayoutTestApplication single_thread(1);
	LayoutTestApplication eight_threads(8);

	single_thread.load(lines);
	eight_threads.load(lines);

	EXPECT_EQ(single_thread.layoutThreadCount(), 1u);
	EXPECT_EQ(eight_threads.layoutThreadCount(), 8u);
	LayoutSnapshot const single = single_thread.snapshot();
	LayoutSnapshot const parallel = eight_threads.snapshot();
	EXPECT_TRUE(single.map_valid);
	EXPECT_EQ(single.total_logical_rows, lines.size());
	EXPECT_GT(single.total_visual_rows, single.total_logical_rows);
	EXPECT_TRUE(single == parallel);
}

int main(int argc, char **argv)
{
	qputenv("QT_QPA_PLATFORM", "offscreen");
	QApplication app(argc, argv);
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
