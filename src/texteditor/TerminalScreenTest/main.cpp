#include "../TerminalScreen.h"

#include <gtest/gtest.h>

using texteditor::terminal::TerminalScreen;

namespace {

std::u32string active_text(TerminalScreen const &screen, size_t row)
{
	std::u32string result;
	for (size_t column = 0; column < screen.columns(); column++) {
		auto const &cell = screen.cell(row, column);
		if (cell.continuation) continue;
		result.push_back(cell.character);
		result += cell.combining;
	}
	while (!result.empty() && result.back() == U' ') result.pop_back();
	return result;
}

}

TEST(TerminalScreen, CursorLeftOverwritesExistingText)
{
	TerminalScreen screen(20, 4);
	screen.feed("abc\x1b[2DXY");
	EXPECT_EQ(active_text(screen, 0), U"aXY");
}

TEST(TerminalScreen, MovesAcrossRowsAndColumns)
{
	TerminalScreen screen(20, 4);
	screen.feed("first\r\nsecond\x1b[1A\x1b[6C!");
	EXPECT_EQ(active_text(screen, 0), U"first       !");
	EXPECT_EQ(active_text(screen, 1), U"second");
}

TEST(TerminalScreen, KeepsUtf8AndCsiStateAcrossChunks)
{
	TerminalScreen whole(20, 4);
	TerminalScreen chunks(20, 4);
	std::string const input = "A\xe7\x95\x8c\x1b[2D!";
	whole.feed(input);
	chunks.feed(std::string_view(input).substr(0, 2));
	chunks.feed(std::string_view(input).substr(2, 3));
	chunks.feed(std::string_view(input).substr(5, 2));
	chunks.feed(std::string_view(input).substr(7));
	EXPECT_EQ(chunks.rendered_lines(), whole.rendered_lines());
	EXPECT_EQ(active_text(chunks, 0), U"A!");
}

TEST(TerminalScreen, CursorMovementLeavesBlankCells)
{
	TerminalScreen screen(20, 4);
	screen.feed("\x1b[6GX");
	EXPECT_EQ(active_text(screen, 0), U"     X");
	EXPECT_EQ(screen.cursor_column(), 6u);
}

TEST(TerminalScreen, ClampsLargeMovementToScreenEdges)
{
	TerminalScreen screen(10, 3);
	screen.feed("\x1b[999B\x1b[999CX\x1b[999A\x1b[999D!");
	EXPECT_EQ(screen.cell(2, 9).character, U'X');
	EXPECT_EQ(screen.cell(0, 0).character, U'!');
}

TEST(TerminalScreen, HandlesCarriageReturnBackspaceTabAndLineFeedSeparately)
{
	TerminalScreen screen(20, 4);
	screen.feed("abc\rX\bY\tZ\nQ");
	EXPECT_EQ(active_text(screen, 0), U"Ybc     Z");
	EXPECT_EQ(active_text(screen, 1), U"         Q");
}

TEST(TerminalScreen, MaintainsWideAndCombiningCellInvariants)
{
	TerminalScreen screen(10, 3);
	screen.feed("e\xcc\x81\xe7\x95\x8c");
	EXPECT_EQ(screen.cell(0, 0).character, U'e');
	ASSERT_EQ(screen.cell(0, 0).combining.size(), 1u);
	EXPECT_EQ(screen.cell(0, 0).combining[0], U'\x301');
	EXPECT_EQ(screen.cell(0, 1).character, U'界');
	EXPECT_TRUE(screen.cell(0, 2).continuation);

	screen.feed("\x1b[1DX");
	EXPECT_EQ(screen.cell(0, 1).character, U' ');
	EXPECT_FALSE(screen.cell(0, 1).continuation);
	EXPECT_EQ(screen.cell(0, 2).character, U'X');
	EXPECT_FALSE(screen.cell(0, 2).continuation);
}

TEST(TerminalScreen, AppliesForegroundBackgroundAndExtendedColors)
{
	TerminalScreen screen(20, 4);
	screen.feed("\x1b[31;44mA\x1b[38;5;196;48;2;1;2;3mB\x1b[0mC");
	EXPECT_TRUE(screen.cell(0, 0).style.foreground_specified);
	EXPECT_EQ(screen.cell(0, 0).style.foreground, 0x800000u);
	EXPECT_EQ(screen.cell(0, 0).style.background, 0x000080u);
	EXPECT_EQ(screen.cell(0, 1).style.foreground, 0xff0000u);
	EXPECT_EQ(screen.cell(0, 1).style.background, 0x010203u);
	EXPECT_FALSE(screen.cell(0, 2).style.foreground_specified);
	EXPECT_FALSE(screen.cell(0, 2).style.background_specified);
}

TEST(TerminalScreen, SupportsSavedCursorVisibilityAndAutoWrapModes)
{
	TerminalScreen screen(5, 3);
	screen.feed("A\x1b[s\x1b[3;5HZ\x1b[uB\x1b[?25l\x1b[?7l");
	EXPECT_EQ(active_text(screen, 0), U"AB");
	EXPECT_EQ(screen.cell(2, 4).character, U'Z');
	EXPECT_FALSE(screen.cursor_visible());
	EXPECT_FALSE(screen.auto_wrap());

	screen.feed("\x1b[1;5HXY");
	EXPECT_EQ(screen.cell(0, 4).character, U'Y');
}

TEST(TerminalScreen, ScrollsAtBottomAndResizesWithoutReflow)
{
	TerminalScreen screen(6, 2);
	screen.feed("one\r\ntwo\r\nthree");
	EXPECT_EQ(screen.scrollback_count(), 1u);
	EXPECT_EQ(active_text(screen, 0), U"two");
	EXPECT_EQ(active_text(screen, 1), U"three");
	screen.resize(4, 3);
	EXPECT_EQ(screen.columns(), 4u);
	EXPECT_EQ(screen.row_count(), 3u);
	EXPECT_EQ(active_text(screen, 0), U"two");
	EXPECT_EQ(active_text(screen, 1), U"thre");
}

TEST(TerminalScreen, DiscardsUnsupportedCsiAndOscSequences)
{
	TerminalScreen screen(20, 3);
	screen.feed("A\x1b[2J\x1b]0;title\aB");
	EXPECT_EQ(active_text(screen, 0), U"AB");
}
