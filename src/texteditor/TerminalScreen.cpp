#include "TerminalScreen.h"
#include "UnicodeWidth.h"

#include <algorithm>
#include <limits>

namespace texteditor::terminal {

namespace {

constexpr uint32_t colors[] = {
	0x000000, 0x800000, 0x008000, 0x808000,
	0x000080, 0x800080, 0x008080, 0xc0c0c0,
};

constexpr uint32_t bright_colors[] = {
	0x808080, 0xff0000, 0x00ff00, 0xffff00,
	0x0000ff, 0xff00ff, 0x00ffff, 0xffffff,
};

uint32_t indexed_color(int index)
{
	if (index < 0 || index > 255) return 0;
	if (index < 8) return colors[index];
	if (index < 16) return bright_colors[index - 8];
	if (index < 232) {
		int const cube = index - 16;
		auto component = [](int value){ return value == 0 ? 0 : 55 + value * 40; };
		int const red = component(cube / 36);
		int const green = component((cube / 6) % 6);
		int const blue = component(cube % 6);
		return static_cast<uint32_t>((red << 16) | (green << 8) | blue);
	}
	int const gray = 8 + (index - 232) * 10;
	return static_cast<uint32_t>((gray << 16) | (gray << 8) | gray);
}

bool is_combining(char32_t c)
{
	return (c >= 0x0300 && c <= 0x036f)
		|| (c >= 0x1ab0 && c <= 0x1aff)
		|| (c >= 0x1dc0 && c <= 0x1dff)
		|| (c >= 0x20d0 && c <= 0x20ff)
		|| (c >= 0xfe20 && c <= 0xfe2f)
		|| (c >= 0xfe00 && c <= 0xfe0f)
		|| (c >= 0xe0100 && c <= 0xe01ef)
		|| c == 0x200d;
}

int character_width(char32_t c)
{
	if (is_combining(c)) return 0;
	return std::clamp(UnicodeWidth::width(UnicodeWidth::type(c)), 1, 2);
}

int parse_parameter(std::string_view value)
{
	if (value.empty()) return 0;
	int result = 0;
	for (char c : value) {
		if (c < '0' || c > '9') return -1;
		int const digit = c - '0';
		if (result > (std::numeric_limits<int>::max() - digit) / 10) {
			return std::numeric_limits<int>::max();
		}
		result = result * 10 + digit;
	}
	return result;
}

std::vector<int> parse_parameters(std::string_view value)
{
	std::vector<int> result;
	size_t begin = 0;
	while (true) {
		size_t end = value.find(';', begin);
		if (end == std::string_view::npos) end = value.size();
		result.push_back(parse_parameter(value.substr(begin, end - begin)));
		if (end == value.size()) break;
		begin = end + 1;
	}
	return result;
}

int movement_parameter(std::vector<int> const &parameters, size_t index = 0)
{
	if (index >= parameters.size() || parameters[index] <= 0) return 1;
	return parameters[index];
}

void append_utf8(std::string *output, char32_t c)
{
	if (c <= 0x7f) {
		output->push_back(static_cast<char>(c));
	} else if (c <= 0x7ff) {
		output->push_back(static_cast<char>(0xc0 | (c >> 6)));
		output->push_back(static_cast<char>(0x80 | (c & 0x3f)));
	} else if (c <= 0xffff) {
		output->push_back(static_cast<char>(0xe0 | (c >> 12)));
		output->push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3f)));
		output->push_back(static_cast<char>(0x80 | (c & 0x3f)));
	} else {
		output->push_back(static_cast<char>(0xf0 | (c >> 18)));
		output->push_back(static_cast<char>(0x80 | ((c >> 12) & 0x3f)));
		output->push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3f)));
		output->push_back(static_cast<char>(0x80 | (c & 0x3f)));
	}
}

void append_color_parameter(std::string *output, char const *prefix, uint32_t color)
{
	*output += prefix;
	*output += std::to_string((color >> 16) & 0xff);
	output->push_back(';');
	*output += std::to_string((color >> 8) & 0xff);
	output->push_back(';');
	*output += std::to_string(color & 0xff);
}

void append_style(std::string *output, TerminalStyle const &style)
{
	*output += "\x1b[0";
	if (style.foreground_specified) {
		append_color_parameter(output, ";38;2;", style.foreground);
	}
	if (style.background_specified) {
		append_color_parameter(output, ";48;2;", style.background);
	}
	output->push_back('m');
}

}

TerminalParser::TerminalParser()
{
	sequence_.reserve(32);
}

void TerminalParser::reset()
{
	state_ = State::Ground;
	sequence_.clear();
	utf8_value_ = 0;
	utf8_minimum_ = 0;
	utf8_remaining_ = 0;
}

void TerminalParser::feed(TerminalScreen *screen, std::string_view bytes)
{
	if (!screen) return;
	for (unsigned char byte : bytes) {
		process_byte(screen, byte);
	}
}

void TerminalParser::emit_codepoint(TerminalScreen *screen, char32_t codepoint)
{
	screen->put_codepoint(codepoint);
}

void TerminalParser::process_ground_byte(TerminalScreen *screen, unsigned char byte)
{
	if (utf8_remaining_ > 0) {
		if ((byte & 0xc0) == 0x80) {
			utf8_value_ = (utf8_value_ << 6) | (byte & 0x3f);
			utf8_remaining_--;
			if (utf8_remaining_ == 0) {
				uint32_t const value = utf8_value_;
				if (value < utf8_minimum_ || value > 0x10ffff
					|| (value >= 0xd800 && value <= 0xdfff)) {
					emit_codepoint(screen, 0xfffd);
				} else {
					emit_codepoint(screen, static_cast<char32_t>(value));
				}
			}
			return;
		}
		emit_codepoint(screen, 0xfffd);
		utf8_remaining_ = 0;
		utf8_value_ = 0;
		utf8_minimum_ = 0;
		process_ground_byte(screen, byte);
		return;
	}

	if (byte == 0x1b) {
		state_ = State::Escape;
		return;
	}
	switch (byte) {
	case '\n': screen->line_feed(); return;
	case '\r': screen->carriage_return(); return;
	case '\b': screen->backspace(); return;
	case '\t': screen->horizontal_tab(); return;
	case 0x00:
	case 0x07:
	case 0x7f:
		return;
	default:
		break;
	}
	if (byte < 0x20) return;
	if (byte < 0x80) {
		emit_codepoint(screen, byte);
	} else if (byte >= 0xc2 && byte <= 0xdf) {
		utf8_value_ = byte & 0x1f;
		utf8_minimum_ = 0x80;
		utf8_remaining_ = 1;
	} else if (byte >= 0xe0 && byte <= 0xef) {
		utf8_value_ = byte & 0x0f;
		utf8_minimum_ = 0x800;
		utf8_remaining_ = 2;
	} else if (byte >= 0xf0 && byte <= 0xf4) {
		utf8_value_ = byte & 0x07;
		utf8_minimum_ = 0x10000;
		utf8_remaining_ = 3;
	} else {
		emit_codepoint(screen, 0xfffd);
	}
}

void TerminalParser::process_byte(TerminalScreen *screen, unsigned char byte)
{
	switch (state_) {
	case State::Ground:
		process_ground_byte(screen, byte);
		break;
	case State::Escape:
		if (byte == '[') {
			sequence_.clear();
			state_ = State::Csi;
		} else if (byte == ']') {
			state_ = State::Osc;
		} else if (byte == '7') {
			screen->save_cursor();
			state_ = State::Ground;
		} else if (byte == '8') {
			screen->restore_cursor();
			state_ = State::Ground;
		} else if (byte == 0x1b) {
			state_ = State::Escape;
		} else {
			state_ = State::Ground;
		}
		break;
	case State::Csi:
		if (byte == 0x1b) {
			state_ = State::Escape;
			sequence_.clear();
		} else if (byte >= 0x40 && byte <= 0x7e) {
			screen->dispatch_csi(sequence_, static_cast<char>(byte));
			sequence_.clear();
			state_ = State::Ground;
		} else if (sequence_.size() < 1024) {
			sequence_.push_back(static_cast<char>(byte));
		} else {
			sequence_.clear();
			state_ = State::Ground;
		}
		break;
	case State::Osc:
		if (byte == 0x07) state_ = State::Ground;
		else if (byte == 0x1b) state_ = State::OscEscape;
		break;
	case State::OscEscape:
		state_ = byte == '\\' ? State::Ground : State::Osc;
		break;
	}
}

TerminalScreen::TerminalScreen(size_t columns, size_t rows)
	: columns_(std::max<size_t>(1, columns))
	, rows_(std::max<size_t>(1, rows))
{
	active_rows_.assign(rows_, blank_row());
}

TerminalScreen::Row TerminalScreen::blank_row() const
{
	return Row(columns_);
}

void TerminalScreen::reset()
{
	active_rows_.assign(rows_, blank_row());
	scrollback_.clear();
	cursor_row_ = 0;
	cursor_column_ = 0;
	saved_cursor_row_ = 0;
	saved_cursor_column_ = 0;
	style_ = {};
	cursor_visible_ = true;
	auto_wrap_ = true;
	pending_wrap_ = false;
	alternate_screen_active_ = false;
	last_feed_scrolled_ = false;
	pending_responses_.clear();
	parser_.reset();
}

void TerminalScreen::normalize_row(Row *row) const
{
	if (!row) return;
	for (size_t column = 0; column < row->size(); column++) {
		TerminalCell &cell = (*row)[column];
		if (cell.continuation) {
			if (column == 0 || (*row)[column - 1].continuation
				|| character_width((*row)[column - 1].character) != 2) {
				cell = {};
			}
		} else if (character_width(cell.character) == 2
			&& (column + 1 >= row->size() || !(*row)[column + 1].continuation)) {
			cell = {};
		}
	}
}

void TerminalScreen::resize(size_t columns, size_t rows)
{
	columns = std::max<size_t>(1, columns);
	rows = std::max<size_t>(1, rows);
	if (columns == columns_ && rows == rows_) return;
	columns_ = columns;
	for (Row &row : active_rows_) {
		row.resize(columns_);
		normalize_row(&row);
	}
	for (Row &row : scrollback_) {
		row.resize(columns_);
		normalize_row(&row);
	}
	rows_ = rows;
	active_rows_.resize(rows_, blank_row());
	cursor_row_ = std::min(cursor_row_, rows_ - 1);
	cursor_column_ = std::min(cursor_column_, columns_ - 1);
	saved_cursor_row_ = std::min(saved_cursor_row_, rows_ - 1);
	saved_cursor_column_ = std::min(saved_cursor_column_, columns_ - 1);
	pending_wrap_ = false;
}

void TerminalScreen::feed(std::string_view bytes)
{
	last_feed_scrolled_ = false;
	parser_.feed(this, bytes);
}

size_t TerminalScreen::columns() const { return columns_; }
size_t TerminalScreen::row_count() const { return rows_; }
size_t TerminalScreen::scrollback_count() const { return scrollback_.size(); }
size_t TerminalScreen::cursor_row() const { return cursor_row_; }
size_t TerminalScreen::cursor_column() const { return cursor_column_; }
size_t TerminalScreen::cursor_document_row() const { return scrollback_.size() + cursor_row_; }
bool TerminalScreen::cursor_visible() const { return cursor_visible_; }
bool TerminalScreen::auto_wrap() const { return auto_wrap_; }
bool TerminalScreen::alternate_screen_active() const { return alternate_screen_active_; }
bool TerminalScreen::last_feed_scrolled() const { return last_feed_scrolled_; }
TerminalStyle const &TerminalScreen::current_style() const { return style_; }

std::string TerminalScreen::take_pending_responses()
{
	std::string result = std::move(pending_responses_);
	pending_responses_.clear();
	return result;
}

TerminalCell const &TerminalScreen::cell(size_t row, size_t column) const
{
	return active_rows_.at(row).at(column);
}

size_t TerminalScreen::cursor_text_column() const
{
	Row const &row = active_rows_[cursor_row_];
	size_t result = 0;
	for (size_t column = 0; column < cursor_column_ && column < row.size(); column++) {
		TerminalCell const &cell = row[column];
		if (!cell.continuation) result += 1 + cell.combining.size();
	}
	return result;
}

void TerminalScreen::clear_cell(size_t row, size_t column)
{
	if (row >= rows_ || column >= columns_) return;
	Row &line = active_rows_[row];
	if (line[column].continuation && column > 0) {
		line[column - 1] = {};
		line[column] = {};
		return;
	}
	if (column + 1 < columns_ && line[column + 1].continuation) {
		line[column + 1] = {};
	}
	line[column] = {};
}

void TerminalScreen::put_codepoint(char32_t codepoint)
{
	int const width = character_width(codepoint);
	if (width == 0) {
		size_t row = cursor_row_;
		size_t column = cursor_column_;
		if (!pending_wrap_) {
			if (column == 0) {
				if (row == 0) return;
				row--;
				column = columns_ - 1;
			} else {
				column--;
			}
		}
		Row &line = active_rows_[row];
		if (line[column].continuation && column > 0) column--;
		line[column].combining.push_back(codepoint);
		return;
	}
	if (pending_wrap_) {
		if (auto_wrap_) {
			line_feed();
			cursor_column_ = 0;
		}
		pending_wrap_ = false;
	}
	if (width == 2 && cursor_column_ + 1 >= columns_) {
		if (!auto_wrap_ || columns_ < 2) return;
		line_feed();
		cursor_column_ = 0;
	}

	clear_cell(cursor_row_, cursor_column_);
	TerminalCell &target = active_rows_[cursor_row_][cursor_column_];
	target.character = codepoint;
	target.style = style_;
	target.continuation = false;
	if (width == 2) {
		clear_cell(cursor_row_, cursor_column_ + 1);
		TerminalCell &continuation = active_rows_[cursor_row_][cursor_column_ + 1];
		continuation.style = style_;
		continuation.continuation = true;
	}

	if (cursor_column_ + static_cast<size_t>(width) >= columns_) {
		cursor_column_ = columns_ - 1;
		pending_wrap_ = auto_wrap_;
	} else {
		cursor_column_ += static_cast<size_t>(width);
	}
}

void TerminalScreen::line_feed()
{
	pending_wrap_ = false;
	if (cursor_row_ + 1 < rows_) {
		cursor_row_++;
		return;
	}
	scrollback_.push_back(std::move(active_rows_.front()));
	active_rows_.erase(active_rows_.begin());
	active_rows_.push_back(blank_row());
	last_feed_scrolled_ = true;
}

void TerminalScreen::carriage_return()
{
	cursor_column_ = 0;
	pending_wrap_ = false;
}

void TerminalScreen::backspace()
{
	if (cursor_column_ > 0) cursor_column_--;
	pending_wrap_ = false;
}

void TerminalScreen::horizontal_tab()
{
	cursor_column_ = std::min(columns_ - 1, ((cursor_column_ / 8) + 1) * 8);
	pending_wrap_ = false;
}

void TerminalScreen::erase_in_display(int mode)
{
	auto erase_range = [this](size_t first_row, size_t first_column,
		size_t last_row, size_t last_column){
		for (size_t row = first_row; row <= last_row; row++) {
			size_t const begin = row == first_row ? first_column : 0;
			size_t const end = row == last_row ? last_column : columns_ - 1;
			for (size_t column = begin; column <= end; column++) {
				clear_cell(row, column);
			}
		}
	};

	switch (mode) {
	case 0:
		erase_range(cursor_row_, cursor_column_, rows_ - 1, columns_ - 1);
		break;
	case 1:
		erase_range(0, 0, cursor_row_, cursor_column_);
		break;
	case 2:
		erase_range(0, 0, rows_ - 1, columns_ - 1);
		break;
	case 3:
		// xterm extension: CSI 3 J removes saved history but not the active
		// screen.  It is useful when a shell's clear-scrollback command is used.
		scrollback_.clear();
		break;
	default:
		break;
	}
}

void TerminalScreen::erase_in_line(int mode)
{
	size_t first = 0;
	size_t last = columns_ - 1;
	if (mode == 0) {
		first = cursor_column_;
	} else if (mode == 1) {
		last = cursor_column_;
	} else if (mode != 2) {
		return;
	}
	for (size_t column = first; column <= last; column++) {
		clear_cell(cursor_row_, column);
	}
}

void TerminalScreen::queue_response(std::string_view response)
{
	pending_responses_.append(response.data(), response.size());
}

void TerminalScreen::save_cursor()
{
	saved_cursor_row_ = cursor_row_;
	saved_cursor_column_ = cursor_column_;
}

void TerminalScreen::restore_cursor()
{
	cursor_row_ = std::min(saved_cursor_row_, rows_ - 1);
	cursor_column_ = std::min(saved_cursor_column_, columns_ - 1);
	pending_wrap_ = false;
}

void TerminalScreen::update_sgr(std::vector<int> const &parameters)
{
	std::vector<int> values = parameters;
	if (values.empty()) values.push_back(0);
	for (size_t i = 0; i < values.size(); i++) {
		int const value = values[i];
		if (value == 0) {
			style_ = {};
		} else if (value >= 30 && value <= 37) {
			style_.foreground_specified = true;
			style_.foreground = colors[value - 30];
		} else if (value == 39) {
			style_.foreground_specified = false;
			style_.foreground = 0;
		} else if (value >= 40 && value <= 47) {
			style_.background_specified = true;
			style_.background = colors[value - 40];
		} else if (value == 49) {
			style_.background_specified = false;
			style_.background = 0;
		} else if (value >= 90 && value <= 97) {
			style_.foreground_specified = true;
			style_.foreground = bright_colors[value - 90];
		} else if (value >= 100 && value <= 107) {
			style_.background_specified = true;
			style_.background = bright_colors[value - 100];
		} else if (value == 38 || value == 48) {
			bool const foreground = value == 38;
			if (i + 1 >= values.size()) continue;
			int const selector = values[++i];
			bool specified = false;
			uint32_t color = 0;
			if (selector == 5 && i + 1 < values.size()) {
				int const index = values[++i];
				if (index >= 0 && index <= 255) {
					color = indexed_color(index);
					specified = true;
				}
			} else if (selector == 2 && i + 3 < values.size()) {
				int const red = values[i + 1];
				int const green = values[i + 2];
				int const blue = values[i + 3];
				i += 3;
				if (red >= 0 && red <= 255 && green >= 0 && green <= 255
					&& blue >= 0 && blue <= 255) {
					color = static_cast<uint32_t>((red << 16) | (green << 8) | blue);
					specified = true;
				}
			}
			if (specified) {
				if (foreground) {
					style_.foreground_specified = true;
					style_.foreground = color;
				} else {
					style_.background_specified = true;
					style_.background = color;
				}
			}
		}
	}
}

void TerminalScreen::dispatch_csi(std::string_view parameters, char command)
{
	bool private_mode = !parameters.empty() && parameters.front() == '?';
	if (private_mode) parameters.remove_prefix(1);
	std::vector<int> const values = parse_parameters(parameters);
	auto clamp_row = [this](long long value){
		return static_cast<size_t>(std::clamp<long long>(value, 0, rows_ - 1));
	};
	auto clamp_column = [this](long long value){
		return static_cast<size_t>(std::clamp<long long>(value, 0, columns_ - 1));
	};

	if (private_mode && (command == 'h' || command == 'l')) {
		bool const enabled = command == 'h';
		for (int value : values) {
			if (value == 25) cursor_visible_ = enabled;
			else if (value == 7) auto_wrap_ = enabled;
			else if (value == 47 || value == 1047 || value == 1049) {
				// P3 will give this mode its own cell buffer.  Tracking the request
				// now is enough to keep full-screen TUIs from forcing log follow.
				alternate_screen_active_ = enabled;
			}
		}
		pending_wrap_ = false;
		return;
	}
	if (private_mode) return;
	if (command == 'n' && values.size() == 1) {
		if (values.front() == 5) {
			// Device Status Report: terminal is operating normally.
			queue_response("\x1b[0n");
		} else if (values.front() == 6) {
			// Cursor Position Report uses one-origin row and column values.
			std::string response = "\x1b[";
			response += std::to_string(cursor_row_ + 1);
			response.push_back(';');
			response += std::to_string(cursor_column_ + 1);
			response.push_back('R');
			queue_response(response);
		}
		// Queries must not affect the pending-wrap state of the display.
		return;
	}
	if (command == 'c' && (parameters.empty()
		|| (values.size() == 1 && values.front() == 0))) {
		// Primary Device Attributes: report the conservative VT100 profile.  Do
		// not advertise optional capabilities that this screen does not support.
		queue_response("\x1b[?1;0c");
		return;
	}

	switch (command) {
	case 'A': cursor_row_ = clamp_row(static_cast<long long>(cursor_row_) - movement_parameter(values)); break;
	case 'B': cursor_row_ = clamp_row(static_cast<long long>(cursor_row_) + movement_parameter(values)); break;
	case 'C': cursor_column_ = clamp_column(static_cast<long long>(cursor_column_) + movement_parameter(values)); break;
	case 'D': cursor_column_ = clamp_column(static_cast<long long>(cursor_column_) - movement_parameter(values)); break;
	case 'E':
		cursor_row_ = clamp_row(static_cast<long long>(cursor_row_) + movement_parameter(values));
		cursor_column_ = 0;
		break;
	case 'F':
		cursor_row_ = clamp_row(static_cast<long long>(cursor_row_) - movement_parameter(values));
		cursor_column_ = 0;
		break;
	case 'G': cursor_column_ = clamp_column(static_cast<long long>(movement_parameter(values)) - 1); break;
	case 'J': erase_in_display(values.empty() ? 0 : values.front()); break;
	case 'K': erase_in_line(values.empty() ? 0 : values.front()); break;
	case 'H':
	case 'f':
		cursor_row_ = clamp_row(static_cast<long long>(movement_parameter(values, 0)) - 1);
		cursor_column_ = clamp_column(static_cast<long long>(movement_parameter(values, 1)) - 1);
		break;
	case 's': save_cursor(); break;
	case 'u': restore_cursor(); break;
	case 'm': update_sgr(values); break;
	default: break;
	}
	if (command != 'm') pending_wrap_ = false;
}

std::string TerminalScreen::render_row(Row const &row) const
{
	size_t end = 0;
	for (size_t i = 0; i < row.size(); i++) {
		TerminalCell const &cell = row[i];
		if (!cell.continuation
			&& (cell.character != U' ' || !cell.combining.empty()
				|| cell.style != TerminalStyle {})) {
			end = i + 1;
		}
	}
	std::string output;
	TerminalStyle current;
	for (size_t i = 0; i < end; i++) {
		TerminalCell const &cell = row[i];
		if (cell.continuation) continue;
		if (cell.style != current) {
			append_style(&output, cell.style);
			current = cell.style;
		}
		append_utf8(&output, cell.character);
		for (char32_t combining : cell.combining) append_utf8(&output, combining);
	}
	if (current != TerminalStyle {}) output += "\x1b[0m";
	return output;
}

std::vector<std::string> TerminalScreen::rendered_lines() const
{
	std::vector<std::string> result;
	result.reserve(scrollback_.size() + active_rows_.size());
	for (Row const &row : scrollback_) result.push_back(render_row(row));
	for (Row const &row : active_rows_) result.push_back(render_row(row));
	return result;
}

}
