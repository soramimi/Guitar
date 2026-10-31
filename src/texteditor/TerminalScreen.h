#ifndef TERMINALSCREEN_H
#define TERMINALSCREEN_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace texteditor::terminal {

struct TerminalStyle {
	bool foreground_specified = false;
	bool background_specified = false;
	uint32_t foreground = 0;
	uint32_t background = 0;

	bool operator == (TerminalStyle const &other) const
	{
		return foreground_specified == other.foreground_specified
			&& background_specified == other.background_specified
			&& foreground == other.foreground
			&& background == other.background;
	}
	bool operator != (TerminalStyle const &other) const { return !(*this == other); }
};

struct TerminalCell {
	char32_t character = U' ';
	std::u32string combining;
	TerminalStyle style;
	bool continuation = false;

	bool operator == (TerminalCell const &other) const
	{
		return character == other.character && combining == other.combining
			&& style == other.style && continuation == other.continuation;
	}
};

class TerminalScreen;

class TerminalParser {
public:
	TerminalParser();
	void reset();
	void feed(TerminalScreen *screen, std::string_view bytes);

private:
	enum class State {
		Ground,
		Escape,
		Csi,
		Osc,
		OscEscape,
	};

	State state_ = State::Ground;
	std::string sequence_;
	uint32_t utf8_value_ = 0;
	uint32_t utf8_minimum_ = 0;
	int utf8_remaining_ = 0;

	void process_byte(TerminalScreen *screen, unsigned char byte);
	void process_ground_byte(TerminalScreen *screen, unsigned char byte);
	void emit_codepoint(TerminalScreen *screen, char32_t codepoint);
};

class TerminalScreen {
public:
	TerminalScreen(size_t columns = 80, size_t rows = 25);

	void reset();
	void resize(size_t columns, size_t rows);
	void feed(std::string_view bytes);

	size_t columns() const;
	size_t row_count() const;
	size_t scrollback_count() const;
	size_t cursor_row() const;
	size_t cursor_column() const;
	size_t cursor_document_row() const;
	size_t cursor_text_column() const;
	bool cursor_visible() const;
	bool auto_wrap() const;
	// This is a mode marker only until P3 implements separate buffers.  It lets
	// the view avoid treating full-screen applications as scrolling logs.
	bool alternate_screen_active() const;
	// True only when the most recent feed() scrolled the active screen upward.
	bool last_feed_scrolled() const;
	TerminalCell const &cell(size_t row, size_t column) const;
	TerminalStyle const &current_style() const;
	std::vector<std::string> rendered_lines() const;
	// Returns terminal replies generated while parsing output, then clears them.
	// Callers must send these bytes back through the PTY input path; they are
	// protocol data and must never be projected as terminal text.
	std::string take_pending_responses();

private:
	friend class TerminalParser;
	using Row = std::vector<TerminalCell>;

	size_t columns_ = 80;
	size_t rows_ = 25;
	std::vector<Row> active_rows_;
	std::vector<Row> scrollback_;
	size_t cursor_row_ = 0;
	size_t cursor_column_ = 0;
	size_t saved_cursor_row_ = 0;
	size_t saved_cursor_column_ = 0;
	TerminalStyle style_;
	bool cursor_visible_ = true;
	bool auto_wrap_ = true;
	bool pending_wrap_ = false;
	bool alternate_screen_active_ = false;
	bool last_feed_scrolled_ = false;
	std::string pending_responses_;
	TerminalParser parser_;

	Row blank_row() const;
	void normalize_row(Row *row) const;
	void clear_cell(size_t row, size_t column);
	void put_codepoint(char32_t codepoint);
	void line_feed();
	void carriage_return();
	void backspace();
	void horizontal_tab();
	void dispatch_csi(std::string_view parameters, char command);
	void erase_in_display(int mode);
	void erase_in_line(int mode);
	void queue_response(std::string_view response);
	void update_sgr(std::vector<int> const &parameters);
	void save_cursor();
	void restore_cursor();
	std::string render_row(Row const &row) const;
};

}

#endif // TERMINALSCREEN_H
