#include "../AbstractTextEditorApplication.h"
#include "../EscapeSequence.h"

#include <gtest/gtest.h>

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
