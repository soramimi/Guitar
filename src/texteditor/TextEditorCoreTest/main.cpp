#include "../AbstractTextEditorApplication.h"
#include "../EscapeSequence.h"
#include "../TextEditorView.h"
#include "../TextEditorApp/src/PtyProcessController.h"

#include <QApplication>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QThread>
#include <QTimer>
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

	std::vector<Character> flattened_visual_characters() const
	{
		std::vector<Character> result;
		for (row_index_t vrow = 0; vrow < visual_nlines(); vrow++) {
			CharBuffer *chars = parse_line(vrow);
			if (!chars) continue;
			result.insert(result.end(), chars->begin(), chars->end());
		}
		return result;
	}

	row_index_t visual_line_count() const
	{
		return visual_nlines();
	}

	std::vector<Character> logical_characters(row_index_t row) const
	{
		CharBuffer chars = parse_logical_line(cx(), row);
		return *chars.vec;
	}

	void feed_terminal(std::string_view bytes)
	{
		appendBulk(bytes);
	}

	void resize_terminal(int columns, int rows)
	{
		setTerminalScreenSize(columns, rows);
	}

	texteditor::RowCol terminal_cursor() const
	{
		return terminalCursorPosition();
	}

	void move_editor_cursor(texteditor::RowCol position)
	{
		setCursorPos(position);
	}

	bool line_index_map_valid() const
	{
		return cx()->line_index_map.validate();
	}

	size_t logical_line_count() const
	{
		return document()->logical_lines.size();
	}

	bool visual_fragment_positions_match_text_width() const
	{
		for (row_index_t vrow = 0; vrow < visual_nlines(); vrow++) {
			CharBuffer *chars = parse_line(vrow);
			if (!chars || chars->empty()) continue;
			QString text;
			for (Character const &ch : *chars->vec) {
				text.append(QString::fromUcs4(&ch.unicode, 1));
			}
			if (chars->left_x(0, text_font()) != 0) return false;
			if (chars->right_x(static_cast<int>(chars->size()) - 1, text_font()) != text_font().text_width(text)) return false;
		}
		return true;
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

class FontInitializationTestApplication : public AbstractTextEditorApplication {
public:
	void set_fonts_before_initialization(QFont const &font)
	{
		set_fixed_font(font);
		set_text_font(font);
	}

	void initialize()
	{
		initEditor();
	}

	int text_tab_stop_distance() const
	{
		return text_font().tab_stop_distance();
	}

	int expected_tab_stop_distance() const
	{
		return fixed_font().basis_char_width() * 4;
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

class InputTestView : public TextEditorView {
public:
	bool document_empty()
	{
		return document()->logical_lines.empty();
	}

	size_t logical_line_count()
	{
		return document()->logical_lines.size();
	}

	std::string first_line_text()
	{
		if (document()->logical_lines.empty()) return {};
		return std::string(document()->logical_lines.front().text());
	}
};

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

TEST(Font, CanBeConfiguredBeforeEditorInitialization)
{
	FontInitializationTestApplication app;
	QFont font("DejaVu Sans");
	font.setPixelSize(14);

	app.set_fonts_before_initialization(font);
	app.initialize();

	EXPECT_EQ(app.text_tab_stop_distance(), app.expected_tab_stop_distance());
}

TEST(CharBuffer, CalculatesAllXPositionsWithLayout)
{
	texteditor::Font font;
	QFont qfont("DejaVu Sans");
	qfont.setPixelSize(14);
	font.set_font(qfont);
	font.set_tab_stop_distance(font.basis_char_width() * 4);

	std::vector<Character> source{
		Character(U'A'),
		Character(U'V'), // カーニングがある組み合わせ
		Character(U'\t'),
		Character(U'e'),
		Character(U'\u0301'), // 結合文字
		Character(U'\U0001f600'),
	};
	CharBuffer chars(source);

	// 最初の問い合わせで、行全体の座標が埋まる。
	EXPECT_EQ(chars.left_x(0, font), 0);
	for (size_t i = 0; i < chars.size(); i++) {
		EXPECT_GE(chars[i].left_x(), 0);
		EXPECT_GE(chars[i].right_x(), chars[i].left_x());
		if (i + 1 < chars.size()) {
			EXPECT_EQ(chars[i].right_x(), chars[i + 1].left_x());
		}
	}

	QString const text = QString::fromUcs4(U"AV\te\u0301\U0001f600");
	EXPECT_EQ(chars.right_x(static_cast<int>(chars.size()) - 1, font), font.text_width(text));
}

TEST(CharBuffer, TreatsLineTerminatorsAsZeroWidth)
{
	texteditor::Font font;
	QFont qfont("DejaVu Sans");
	qfont.setPixelSize(14);
	font.set_font(qfont);

	CharBuffer chars(std::vector<Character>{Character(U'A'), Character(U'\r'), Character(U'\n')});
	EXPECT_EQ(chars.right_x(0, font), chars.left_x(1, font));
	EXPECT_EQ(chars.left_x(1, font), chars.right_x(1, font));
	EXPECT_EQ(chars.left_x(2, font), chars.right_x(2, font));
}

TEST(CharAttr, DistinguishesDefaultAndExplicitBlack)
{
	CharAttr attr;
	EXPECT_FALSE(attr.has_foreground_color());
	EXPECT_FALSE(attr.has_background_color());

	attr.set_foreground_color(0x000000);
	EXPECT_TRUE(attr.has_foreground_color());
	EXPECT_EQ(attr.color, 0u);
	EXPECT_FALSE(attr.has_background_color());

	attr.set_background_color(0x000000);
	EXPECT_TRUE(attr.has_background_color());
	EXPECT_EQ(attr.background_color, 0u);
	EXPECT_TRUE(attr.has_foreground_color());

	attr.clear_foreground_color();
	EXPECT_FALSE(attr.has_foreground_color());
	EXPECT_EQ(attr.color, 0u);
	EXPECT_TRUE(attr.has_background_color());

	attr.clear_background_color();
	EXPECT_FALSE(attr.has_background_color());
	EXPECT_EQ(attr.background_color, 0u);
}

TEST(EscapeSequence, ResetClearsExplicitColor)
{
	EscapeSequence sequence;
	for (char c : std::string("\x1b[30m")) sequence.update(c);
	EXPECT_TRUE(sequence.has_color());
	EXPECT_EQ(sequence.color(), 0u);

	for (char c : std::string("\x1b[0m")) sequence.update(c);
	EXPECT_FALSE(sequence.has_color());
	EXPECT_FALSE(sequence.has_background_color());
}

TEST(EscapeSequence, SupportsEmptyAndMultipleSgrParameters)
{
	EscapeSequence sequence;
	auto update = [&](std::string const &text){
		bool consumed = false;
		for (char c : text) consumed = sequence.update(c);
		return consumed;
	};

	EXPECT_TRUE(update("\x1b[1;31m"));
	EXPECT_TRUE(sequence.has_color());
	EXPECT_EQ(sequence.color(), 0x800000u);

	EXPECT_TRUE(update("\x1b[m"));
	EXPECT_FALSE(sequence.has_color());

	// 色を変えないSGRも、終端の'm'を本文へ流出させない。
	EXPECT_TRUE(update("\x1b[1m"));
	EXPECT_FALSE(sequence.has_color());

	for (char c : std::string("\x1b[36m")) sequence.update(c);
	EXPECT_TRUE(sequence.has_color());
	EXPECT_EQ(sequence.color(), 0x008080u);
	EXPECT_TRUE(update("\x1b[39m"));
	EXPECT_FALSE(sequence.has_color());

	// neofetchが出力するカーソル表示・折り返し制御も、未対応の表示文字として
	// 終端の'h'や'l'を本文へ流出させず、CSI全体を消費する。
	EXPECT_TRUE(update("\x1b[?25l"));
	EXPECT_TRUE(sequence.empty());
	EXPECT_TRUE(update("\x1b[?7h"));
	EXPECT_TRUE(sequence.empty());
	EXPECT_TRUE(update("\x1b[41C"));
	EXPECT_TRUE(sequence.empty());
}

TEST(EscapeSequence, SupportsBackgroundAndExtendedColors)
{
	EscapeSequence sequence;
	auto update = [&](std::string const &text){
		bool consumed = false;
		for (char c : text) consumed = sequence.update(c);
		return consumed;
	};

	EXPECT_TRUE(update("\x1b[31;44m"));
	EXPECT_TRUE(sequence.has_color());
	EXPECT_EQ(sequence.color(), 0x800000u);
	EXPECT_TRUE(sequence.has_background_color());
	EXPECT_EQ(sequence.background_color(), 0x000080u);

	EXPECT_TRUE(update("\x1b[49m"));
	EXPECT_TRUE(sequence.has_color());
	EXPECT_FALSE(sequence.has_background_color());

	EXPECT_TRUE(update("\x1b[48;5;0m"));
	EXPECT_TRUE(sequence.has_background_color());
	EXPECT_EQ(sequence.background_color(), 0x000000u);
	EXPECT_TRUE(update("\x1b[48;5;196m"));
	EXPECT_EQ(sequence.background_color(), 0xff0000u);
	EXPECT_TRUE(update("\x1b[48;5;232m"));
	EXPECT_EQ(sequence.background_color(), 0x080808u);
	EXPECT_TRUE(update("\x1b[103m"));
	EXPECT_EQ(sequence.background_color(), 0xffff00u);

	EXPECT_TRUE(update("\x1b[48;2;10;20;30m"));
	EXPECT_EQ(sequence.background_color(), 0x0a141eu);
	EXPECT_TRUE(update("\x1b[38;5;46m"));
	EXPECT_EQ(sequence.color(), 0x00ff00u);

	uint32_t const saved_background = sequence.background_color();
	EXPECT_TRUE(update("\x1b[48;5m"));
	EXPECT_EQ(sequence.background_color(), saved_background);
	EXPECT_TRUE(update("\x1b[48;2;1;2m"));
	EXPECT_EQ(sequence.background_color(), saved_background);

	EXPECT_TRUE(update("\x1b[0m"));
	EXPECT_FALSE(sequence.has_color());
	EXPECT_FALSE(sequence.has_background_color());
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

TEST(TerminalLayout, ProjectsCursorMovementAndColorsToDocument)
{
	LayoutTestApplication app(1);
	app.setTerminalMode(true);
	app.resize_terminal(20, 4);
	app.feed_terminal("abc\x1b[2D\x1b[31;44mXY\x1b[0m");

	EXPECT_EQ(app.wrappingMode(), AbstractTextEditorApplication::WrappingMode::NoWrap);
	EXPECT_EQ(app.visual_line_count(), 4);
	EXPECT_TRUE(app.line_index_map_valid());
	std::vector<Character> const chars = app.logical_characters(0);
	ASSERT_GE(chars.size(), 3u);
	EXPECT_EQ(chars[0].unicode, U'a');
	EXPECT_EQ(chars[1].unicode, U'X');
	EXPECT_EQ(chars[2].unicode, U'Y');
	for (size_t i = 1; i <= 2; i++) {
		EXPECT_TRUE(chars[i].attr.has_foreground_color());
		EXPECT_EQ(chars[i].attr.color, 0x800000u);
		EXPECT_TRUE(chars[i].attr.has_background_color());
		EXPECT_EQ(chars[i].attr.background_color, 0x000080u);
	}
}

TEST(TerminalLayout, EditorCursorDoesNotChangeTerminalOutputPosition)
{
	LayoutTestApplication app(1);
	app.setTerminalMode(true);
	app.resize_terminal(20, 4);
	app.feed_terminal("abc");
	EXPECT_EQ(app.terminal_cursor().row, 0);
	EXPECT_EQ(app.terminal_cursor().col, 3);

	app.move_editor_cursor({3, 10});
	app.feed_terminal("\x1b[2DX");
	std::vector<Character> const chars = app.logical_characters(0);
	ASSERT_GE(chars.size(), 3u);
	EXPECT_EQ(chars[0].unicode, U'a');
	EXPECT_EQ(chars[1].unicode, U'X');
	EXPECT_EQ(chars[2].unicode, U'c');
	EXPECT_EQ(app.terminal_cursor().row, 0);
	EXPECT_EQ(app.terminal_cursor().col, 2);
	EXPECT_TRUE(app.line_index_map_valid());
}

TEST(TerminalLayout, ProjectsScrollbackWithoutBreakingLineIndexMap)
{
	LayoutTestApplication app(1);
	app.setTerminalMode(true);
	app.resize_terminal(10, 2);
	app.feed_terminal("one\r\ntwo\r\nthree");
	ASSERT_EQ(app.logical_line_count(), 3u);
	EXPECT_TRUE(app.line_index_map_valid());
	auto first = app.logical_characters(0);
	auto second = app.logical_characters(1);
	auto third = app.logical_characters(2);
	ASSERT_GE(first.size(), 3u);
	ASSERT_GE(second.size(), 3u);
	ASSERT_GE(third.size(), 5u);
	EXPECT_EQ(std::u32string({first[0], first[1], first[2]}), U"one");
	EXPECT_EQ(std::u32string({second[0], second[1], second[2]}), U"two");
	EXPECT_EQ(std::u32string({third[0], third[1], third[2], third[3], third[4]}), U"three");
}

#ifndef _WIN32
TEST(PtyProcessController, QueuesLaunchInputAndKeepsChildOutputSeparate)
{
	PtyProcessController controller;
	QByteArray output;
	int exit_code = -1;
	QString error_message;
	bool completed = false;
	QObject::connect(&controller, &PtyProcessController::outputReady,
		[&output](QByteArray const &bytes){ output += bytes; });
	QObject::connect(&controller, &PtyProcessController::completed,
		[&](int code, QString const &error){
			exit_code = code;
			error_message = error;
			completed = true;
		});

	QString const script =
		"stty -echo; IFS= read -r line; printf 'RX=<%s>\\n' $line; stty size";
	ASSERT_TRUE(controller.start("/bin/sh", {"-c", script}, QSize(73, 29)));
	controller.writeInput("launch-input\n");

	QElapsedTimer timer;
	timer.start();
	while (!completed && timer.elapsed() < 5000) {
		QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
		QThread::msleep(1);
	}

	ASSERT_TRUE(completed);
	EXPECT_EQ(exit_code, 0);
	EXPECT_TRUE(error_message.isEmpty());
	EXPECT_EQ(controller.state(), PtyProcessController::State::Exited);
	EXPECT_NE(output.indexOf("RX=<launch-input>\r\n"), -1);
	EXPECT_NE(output.indexOf("29 73\r\n"), -1);
}

TEST(PtyProcessController, RoutesTerminalRepliesBackToTheSamePty)
{
	InputTestView view;
	view.setTextEditorEngine(std::make_shared<TextEditorEngine>());
	view.setTerminalMode(true);
	view.setTerminalScreenSize(80, 25);

	PtyProcessController controller;
	QByteArray output;
	bool completed = false;
	QObject::connect(&controller, &PtyProcessController::outputReady,
		[&](QByteArray const &bytes){
			output += bytes;
			view.appendBulk(std::string_view(bytes.constData(),
				static_cast<size_t>(bytes.size())));
		});
	QObject::connect(&view, &TextEditorView::terminalInputRequested,
		&controller, &PtyProcessController::writeInput);
	QObject::connect(&controller, &PtyProcessController::completed,
		[&](int exit_code, QString const &error){
			EXPECT_EQ(exit_code, 0);
			EXPECT_TRUE(error.isEmpty());
			completed = true;
		});

	QString const script =
		"stty -icanon min 1 time 0 -echo; printf '\\033[6n\\033[c'; "
		"dd bs=1 count=13 2>/dev/null | od -An -tx1";
	ASSERT_TRUE(controller.start("/bin/sh", {"-c", script}, QSize(80, 25)));

	QElapsedTimer timer;
	timer.start();
	while (!completed && timer.elapsed() < 5000) {
		QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
		QThread::msleep(1);
	}

	ASSERT_TRUE(completed);
	EXPECT_NE(output.indexOf("1b 5b 31 3b 31 52 1b 5b 3f 31 3b 30 63"), -1)
		<< output.toHex(' ').constData();
}

TEST(PtyProcessController, UnreadInputDoesNotBlockTheGuiEventLoop)
{
	PtyProcessController controller;
	bool completed = false;
	QObject::connect(&controller, &PtyProcessController::completed,
		[&](int, QString const &){ completed = true; });
	ASSERT_TRUE(controller.start("/bin/sh", {"-c", "stty -echo; sleep 30"}, QSize(80, 25)));

	// The child deliberately never reads. The controller must queue this work
	// away from Qt's event loop, then allow an asynchronous stop to complete.
	controller.writeInput(QByteArray(1024 * 1024, 'x'));
	bool timer_fired = false;
	QTimer::singleShot(50, [&timer_fired](){ timer_fired = true; });
	QElapsedTimer timer;
	timer.start();
	while (!timer_fired && timer.elapsed() < 1000) {
		QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
		QThread::msleep(1);
	}
	EXPECT_TRUE(timer_fired);

	controller.stop();
	timer.restart();
	while (!completed && timer.elapsed() < 5000) {
		QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
		QThread::msleep(1);
	}
	EXPECT_TRUE(completed);
}

TEST(PtyProcessController, PreservesProgramArgumentBoundaries)
{
	PtyProcessController controller;
	QByteArray output;
	bool completed = false;
	QObject::connect(&controller, &PtyProcessController::outputReady,
		[&output](QByteArray const &bytes){ output += bytes; });
	QObject::connect(&controller, &PtyProcessController::completed,
		[&](int exit_code, QString const &error){
			EXPECT_EQ(exit_code, 0);
			EXPECT_TRUE(error.isEmpty());
			completed = true;
		});

	QString const script = "printf '<%s><%s><%s><%s>' \"$1\" \"$2\" \"$3\" \"$4\"";
	ASSERT_TRUE(controller.start("/bin/sh", {
		"-c", script, "argv0", "", "contains space", "quote\"slash\\日本語"}, QSize(80, 25)));
	QElapsedTimer timer;
	timer.start();
	while (!completed && timer.elapsed() < 5000) {
		QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
		QThread::msleep(1);
	}

	ASSERT_TRUE(completed);
	EXPECT_EQ(output, QByteArray("<><contains space><quote\"slash\\") + "日本語><>");
}
#endif

TEST(TextEditorViewInput, TerminalKeysAreRequestedWithoutEditingTheDocument)
{
	InputTestView view;
	view.setTextEditorEngine(std::make_shared<TextEditorEngine>());
	view.setTerminalMode(true);
	size_t const line_count_before = view.logical_line_count();
	QByteArray requested;
	QObject::connect(&view, &TextEditorView::terminalInputRequested,
		[&requested](QByteArray const &bytes){ requested += bytes; });

	QKeyEvent text_event(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, "a");
	QCoreApplication::sendEvent(&view, &text_event);
	QKeyEvent enter_event(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
	QCoreApplication::sendEvent(&view, &enter_event);
	QKeyEvent control_c_event(QEvent::KeyPress, Qt::Key_C, Qt::ControlModifier);
	QCoreApplication::sendEvent(&view, &control_c_event);
	QKeyEvent shortcut_override(QEvent::ShortcutOverride, Qt::Key_C, Qt::ControlModifier);
	shortcut_override.ignore();
	QCoreApplication::sendEvent(&view, &shortcut_override);

	EXPECT_EQ(requested, "a\r\x03");
	EXPECT_EQ(view.logical_line_count(), line_count_before);
	EXPECT_TRUE(shortcut_override.isAccepted());
}

TEST(TextEditorViewInput, TerminalResponsesAreForwardedWithoutBecomingText)
{
	InputTestView view;
	view.setTextEditorEngine(std::make_shared<TextEditorEngine>());
	view.setTerminalMode(true);
	view.setTerminalScreenSize(12, 4);
	QByteArray requested;
	QObject::connect(&view, &TextEditorView::terminalInputRequested,
		[&requested](QByteArray const &bytes){ requested += bytes; });

	view.appendBulk("\x1b[3;4H\x1b[5n\x1b[6n\x1b[c");

	QByteArray const expected("\x1b[0n\x1b[3;4R\x1b[?1;0c");
	EXPECT_EQ(requested, expected);
	EXPECT_EQ(view.first_line_text(), "\n");
}

TEST(TextEditorViewInput, FollowsNormalTerminalScrollbackButNotAlternateScreen)
{
	InputTestView view;
	view.setTextEditorEngine(std::make_shared<TextEditorEngine>());
	view.setTerminalMode(true);
	view.set_client_size(800, 800, true);
	view.setTerminalScreenSize(10, 2);

	auto write_like_main_window = [&view](std::string const &bytes){
		bool const was_viewing_bottom = view.isViewingBottom();
		view.appendBulk(bytes);
		if (was_viewing_bottom && view.terminalOutputScrolled()
			&& !view.terminalAlternateScreenActive()) {
			view.moveToBottom();
		}
	};
	auto lines = [](int count){
		std::string result;
		for (int i = 0; i < count; i++) result += "x\r\n";
		return result;
	};

	write_like_main_window(lines(40));
	EXPECT_TRUE(view.isViewingBottom());
	view.scrollUp();
	view.scrollUp();
	EXPECT_FALSE(view.isViewingBottom());
	write_like_main_window(lines(2));
	EXPECT_FALSE(view.isViewingBottom());

	view.moveToBottom();
	write_like_main_window("\x1b[?1049h" + lines(2));
	EXPECT_TRUE(view.terminalAlternateScreenActive());
	EXPECT_FALSE(view.isViewingBottom());
}

TEST(TextEditorViewInput, NormalEditorKeysStillEditTheDocument)
{
	InputTestView view;
	view.setTextEditorEngine(std::make_shared<TextEditorEngine>());
	view.setNormalTextEditorMode(true);
	QKeyEvent text_event(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, "a");
	QCoreApplication::sendEvent(&view, &text_event);

	ASSERT_FALSE(view.document_empty());
	EXPECT_EQ(view.first_line_text(), "a");
}

int main(int argc, char **argv)
{
	qputenv("QT_QPA_PLATFORM", "offscreen");
	QApplication app(argc, argv);
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
