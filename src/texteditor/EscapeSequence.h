
#ifndef ESCAPESEQUENCE_H
#define ESCAPESEQUENCE_H

#include <string>
#include <string_view>
#include <stdint.h>

// experimental: escape sequence
class EscapeSequence {
private:
	int parse_int(std::string_view sv) const;
	void update_sgr(std::string_view sv);
public:
	std::string chars_;
	uint32_t color_ = 0;
	bool has_color_ = false;
	uint32_t background_color_ = 0;
	bool has_background_color_ = false;
	EscapeSequence();
	void clear();
	bool update(char c);
	bool empty() const;
	uint32_t color() const;
	bool has_color() const;
	uint32_t background_color() const;
	bool has_background_color() const;
};

#endif // ESCAPESEQUENCE_H
