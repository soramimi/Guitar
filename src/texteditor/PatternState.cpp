#include "PatternState.h"
#include "unicode.h"

PatternState::PatternState(const std::string &text)
{
	utf8 u8(text.data(), text.size());
	while (1) {
		char32_t c = u8.next();
		if (c == 0) break;
		chars_.push_back(c);
	}
}

PatternState::State PatternState::match(char32_t c)
{
	if (length_ < chars_.size()) {
		if (c == chars_[length_]) {
			length_++;
			if (length_ == chars_.size()) {
				length_ = 0;
				return Accept;
			}
			return Continue;
		}
	}
	return Fail;
}

size_t PatternState::offset() const
{
	return offset_;
}

void Keywords::first(char32_t c, size_t offset, std::vector<std::unique_ptr<PatternState> > *patterns)
{
	static std::vector<std::string_view> keywords = {
		"if", "else", "for", "while", "switch", "case", "break",
		"continue", "return", "int", "float", "double", "char",
		"void", "struct", "class", "public", "private", "protected",
		"virtual", "override", "const", "static"
	};
	for (auto const &kw : keywords) {
		if (c == kw[0]) {
			auto pattern = std::make_unique<PatternState>(std::string(kw));
			pattern->offset_ = offset;
			pattern->length_ = 1;
			patterns->push_back(std::move(pattern));
		}
	}
}
