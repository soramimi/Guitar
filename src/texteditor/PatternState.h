#ifndef PATTERNSTATE_H
#define PATTERNSTATE_H

#include <stdlib.h>
#include <memory>
#include <vector>
#include <string>

// experimental: キーワード検出
class PatternState {
	friend class Keywords;
private:
	std::vector<char32_t> chars_;
	size_t offset_ = 0;
	size_t length_ = 0;
public:
	PatternState() = default;
	PatternState(std::string const &text);
	enum State {
		Fail,
		Continue,
		Accept,
	};
	State match(char32_t c);
	size_t offset() const;
};

class Keywords {
public:
	void first(char32_t c, size_t offset, std::vector<std::unique_ptr<PatternState>> *patterns);
};

#endif // PATTERNSTATE_H
